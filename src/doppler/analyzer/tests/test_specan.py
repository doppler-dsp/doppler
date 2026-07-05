"""Integration tests for the C ``Specan`` natural-parameter analyzer.

These drive the real ``doppler.analyzer`` extension end to end — the DDC
mix/decimate, the PSD averaging PSD, and the display crop — through its
instrument-parameter API (fs / span / rbw / center / offset_db), the C-first
home for the mapping ``doppler.specan``'s engine used to hand-roll in Python.
"""

import numpy as np
import pytest

from doppler.analyzer import Specan

FS = 1.0e6
SPAN = 200e3
RBW = 1500.0


def _tone(fn: float, n: int = 1 << 16, amp: float = 1.0) -> np.ndarray:
    """A unit-rate complex exponential at normalised frequency ``fn``."""
    return (amp * np.exp(2j * np.pi * fn * np.arange(n))).astype(np.complex64)


def _first_frame(sa: Specan, x: np.ndarray, chunk: int = 4096):
    for i in range(0, len(x), chunk):
        frame = sa.execute(x[i : i + chunk])
        if frame is not None:
            return frame
    raise AssertionError("Specan never produced a frame")


def _bin_hz(sa: Specan, bin_index: int) -> float:
    return (bin_index - sa.display_size // 2) * sa.fs_out / sa.nfft


def test_required_params_are_mandatory():
    """fs/span/rbw are required positionals — omitting them is a TypeError."""
    with pytest.raises(TypeError):
        Specan()  # not a silent NULL → MemoryError
    with pytest.raises(TypeError):
        Specan(fs=FS, span=SPAN)  # rbw missing


def test_natural_params_derive_a_sane_grid():
    sa = Specan(fs=FS, span=SPAN, rbw=RBW, center=0.0)
    assert sa.fs_out == pytest.approx(SPAN * 1.28)
    assert sa.nfft == 2 * sa.n  # pad = 2, n is a power of two
    assert sa.display_size % 2 == 1  # odd → exact DC-centred bin
    assert sa.rbw == pytest.approx(RBW, rel=0.05)  # RBW realised within 5 %
    assert sa.beta > 0.0  # Kaiser actually shaped to hit RBW
    sa.destroy()


def test_tone_lands_at_its_offset_near_0_dbfs():
    sa = Specan(fs=FS, span=SPAN, rbw=RBW, center=0.0)
    frame = _first_frame(sa, _tone(30e3 / FS))
    assert len(frame) == sa.display_size
    peak = int(np.argmax(frame))
    assert _bin_hz(sa, peak) == pytest.approx(30e3, abs=sa.fs_out / sa.nfft)
    assert frame.max() == pytest.approx(
        0.0, abs=1.0
    )  # unit amplitude → 0 dBFS
    assert frame.max() - np.median(frame) > 30.0
    sa.destroy()


def test_offset_db_shifts_the_trace():
    base = Specan(fs=FS, span=SPAN, rbw=RBW, center=0.0)
    shifted = Specan(fs=FS, span=SPAN, rbw=RBW, center=0.0, offset_db=-20.0)
    x = _tone(30e3 / FS)
    fb = _first_frame(base, x)
    fs_ = _first_frame(shifted, x)
    assert fs_.max() == pytest.approx(fb.max() - 20.0, abs=0.5)
    base.destroy()
    shifted.destroy()


def test_bits_sets_the_dbfs_reference():
    """bits=B reads the same as full_scale=2**(B-1) (one source of truth)."""
    bits = 12
    x = _tone(30e3 / FS, amp=10.0)  # below an amplitude-2048 full scale
    fb = _first_frame(Specan(fs=FS, span=SPAN, rbw=RBW, bits=bits), x)
    ffs = _first_frame(
        Specan(fs=FS, span=SPAN, rbw=RBW, full_scale=2 ** (bits - 1)), x
    )
    assert fb.max() == pytest.approx(ffs.max(), abs=1e-3)
    # amplitude 10 against full scale 2048 -> about -46 dBFS peak
    assert fb.max() == pytest.approx(
        20.0 * np.log10(10.0 / 2 ** (bits - 1)), abs=1.0
    )


def test_retune_moves_the_tone_to_dc():
    sa = Specan(fs=FS, span=SPAN, rbw=RBW, center=0.0)
    _first_frame(sa, _tone(30e3 / FS))
    sa.retune(30e3)
    assert sa.center == pytest.approx(30e3)
    frame = _first_frame(sa, _tone(30e3 / FS))
    peak = int(np.argmax(frame))
    assert abs(peak - sa.display_size // 2) <= 2  # at DC
    sa.destroy()


def test_averaging_lowers_the_noise_floor_variance():
    """navg>1 averages segments → a tighter noise-floor estimate."""
    rng = np.random.default_rng(0)
    n = 1 << 18
    noise = (
        (rng.standard_normal(n) + 1j * rng.standard_normal(n)) / np.sqrt(2)
    ).astype(np.complex64)

    sa1 = Specan(fs=FS, span=SPAN, rbw=RBW, center=0.0, navg=1)
    sa8 = Specan(fs=FS, span=SPAN, rbw=RBW, center=0.0, navg=8)
    f1 = _first_frame(sa1, noise)
    f8 = _first_frame(sa8, noise)
    # Averaging 8 segments shrinks the per-bin spread of the noise floor.
    assert np.std(f8) < np.std(f1)
    sa1.destroy()
    sa8.destroy()


def test_execute_returns_none_until_a_frame_is_ready():
    sa = Specan(fs=FS, span=SPAN, rbw=RBW, center=0.0)
    # A handful of input samples decimate to far fewer than one window.
    assert sa.execute(_tone(30e3 / FS, n=64)) is None
    sa.destroy()


def test_context_manager():
    with Specan(fs=FS, span=SPAN, rbw=RBW) as sa:
        assert sa.fs_out > 0.0


def test_execute_out_writes_into_callers_buffer():
    sa = Specan(fs=FS, span=SPAN, rbw=RBW, center=0.0)
    chunk = 4096
    # out= validation requires max(execute_max_out(), len(x)): the kernel's
    # scratch use scales with the input chunk length, not just disp_n.
    out = np.zeros(max(sa.execute_max_out(), chunk), dtype=np.float32)
    x = _tone(30e3 / FS, n=1 << 16)
    frame = None
    for i in range(0, len(x), chunk):
        frame = sa.execute(x[i : i + chunk], out=out)
        if frame is not None:
            break
    assert frame is not None
    assert np.shares_memory(frame, out)
    sa.destroy()


def test_execute_out_undersized_raises():
    sa = Specan(fs=FS, span=SPAN, rbw=RBW, center=0.0)
    out = np.zeros(1, dtype=np.float32)
    with pytest.raises(ValueError):
        sa.execute(_tone(30e3 / FS, n=1 << 16), out=out)
    sa.destroy()
