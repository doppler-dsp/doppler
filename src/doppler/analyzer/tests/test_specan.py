"""Integration tests for the C ``Specan`` natural-parameter analyzer.

These drive the real ``doppler.analyzer`` extension end to end — the DDC
mix/decimate, the PSD averaging PSD, and the display crop — through its
instrument-parameter API (fs / span / rbw / center / ref_db), the C-first home
for the mapping ``doppler.specan``'s engine used to hand-roll in Python.
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


def test_ref_db_offsets_the_trace():
    base = Specan(fs=FS, span=SPAN, rbw=RBW, center=0.0)
    shifted = Specan(fs=FS, span=SPAN, rbw=RBW, center=0.0, ref_db=-20.0)
    x = _tone(30e3 / FS)
    fb = _first_frame(base, x)
    fs_ = _first_frame(shifted, x)
    assert fs_.max() == pytest.approx(fb.max() - 20.0, abs=0.5)
    base.destroy()
    shifted.destroy()


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
