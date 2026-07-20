"""DopplerChannel — clock Doppler as a propagation impairment.

The physics being asserted here, in one line: a Doppler shift rescales the
whole received time base, so the carrier offset and the *clock* rates move
together, from one parameter. Tests that only check the carrier would pass
against an implementation that leaves the code rate untouched — the classic
unphysical shortcut — so the dilation is checked explicitly and separately.

The geometry throughout is SPEC.md's: 3.069 Mcps at ``spc=2`` on a 2.5 GHz
carrier, with the +/-50 kHz uncertainty written as what it physically is,
20 ppm of the time base.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.impairment import DopplerChannel

FS = 6.138e6  # 3.069 Mcps x spc=2
FC = 2.5e9  # SPEC.md nominal carrier
PPM = 20.0  # +/-50 kHz at 2.5 GHz
RATE_PPM_S = 0.2  # 500 Hz/s at 2.5 GHz
N = 1 << 16


def _dc(n: int = N) -> np.ndarray:
    """A DC block: any frequency content in the output is the channel's."""
    return np.ones(n, dtype=np.complex64)


def _peak_hz(y: np.ndarray, fs: float) -> float:
    """Dominant frequency of ``y`` by FFT peak."""
    sp = np.abs(np.fft.fft(y))
    f = np.fft.fftfreq(len(y), 1.0 / fs)
    return float(f[int(np.argmax(sp))])


def test_create() -> None:
    obj = DopplerChannel(1000000.0, 0.0, 0.0, 0.0)
    assert obj is not None


def test_context_manager() -> None:
    with DopplerChannel(1000000.0, 0.0, 0.0, 0.0) as obj:
        assert obj.elapsed_s == 0.0


def test_destroy() -> None:
    obj = DopplerChannel(1000000.0, 0.0, 0.0, 0.0)
    obj.destroy()


def test_carrier_offset_is_fc_times_d() -> None:
    """20 ppm on a 2.5 GHz carrier puts the tone at +50 kHz."""
    ch = DopplerChannel(fs=FS, carrier_hz=FC, doppler_ppm=PPM)
    y = ch.execute(_dc())
    # One FFT bin is fs/N ~= 94 Hz; allow a couple.
    assert _peak_hz(y, FS) == pytest.approx(50_000.0, abs=250.0)
    assert ch.offset_hz == pytest.approx(50_000.0, abs=1.0)


def test_time_base_dilates() -> None:
    """The whole time base is rescaled: n_out ~= n_in / (1 + d).

    This is the half a carrier-only implementation gets wrong. At 20 ppm a
    65536-sample block loses ~1.3 samples -- small, but it is the same 20 ppm
    that shows up as +61.4 chip/s on a 3.069 Mcps code, which a delay-lock
    loop absolutely must track.
    """
    ch = DopplerChannel(fs=FS, carrier_hz=FC, doppler_ppm=PPM)
    y = ch.execute(_dc())
    assert len(y) == pytest.approx(N / (1.0 + PPM * 1e-6), abs=2)


def test_negative_doppler_stretches() -> None:
    """An opening range runs the clocks slow and shifts the carrier down."""
    ch = DopplerChannel(fs=FS, carrier_hz=FC, doppler_ppm=-PPM)
    y = ch.execute(_dc())
    assert len(y) == pytest.approx(N / (1.0 - PPM * 1e-6), abs=2)
    assert _peak_hz(y, FS) == pytest.approx(-50_000.0, abs=250.0)
    assert ch.offset_hz == pytest.approx(-50_000.0, abs=1.0)


def test_zero_doppler_is_pass_through() -> None:
    """d = 0 changes neither the rate nor the carrier."""
    ch = DopplerChannel(fs=FS, carrier_hz=FC, doppler_ppm=0.0)
    y = ch.execute(_dc())
    assert len(y) == N
    assert ch.offset_hz == 0.0


def test_ramp_is_the_integral_not_t_times_d() -> None:
    """offset(t) == fc*d_dot*t, i.e. the integral of the rate -- not twice it.

    The one case that separates a correct implementation from the natural
    wrong one. Accumulating ``t * d(t)`` instead of ``integral d`` passes every
    static-Doppler assertion above and lands at exactly 2x here.
    """
    ch = DopplerChannel(
        fs=FS, carrier_hz=FC, doppler_ppm=0.0, doppler_rate_ppm_s=RATE_PPM_S
    )
    for _ in range(16):
        ch.execute(_dc())
    t = ch.elapsed_s
    assert t > 0.0
    expect = FC * RATE_PPM_S * 1e-6 * t  # 500 Hz/s * t
    assert ch.offset_hz == pytest.approx(expect, rel=1e-9)
    # Guard the specific failure mode, not just the value.
    assert abs(ch.offset_hz - 2.0 * expect) > 0.1 * expect


def test_carrier_hz_is_load_bearing() -> None:
    """fc scales the offset; it is DSP input here, not SigMF metadata.

    Same ppm, two carriers, two different offsets -- and with fc = 0 the
    clocks still dilate while the carrier stays put (permitted, for isolating
    a code loop under test, but not what a real channel does).
    """
    lo = DopplerChannel(fs=FS, carrier_hz=1.0e9, doppler_ppm=PPM)
    hi = DopplerChannel(fs=FS, carrier_hz=2.0e9, doppler_ppm=PPM)
    lo.execute(_dc())
    hi.execute(_dc())
    assert hi.offset_hz == pytest.approx(2.0 * lo.offset_hz, rel=1e-9)

    none = DopplerChannel(fs=FS, carrier_hz=0.0, doppler_ppm=PPM)
    y = none.execute(_dc())
    assert none.offset_hz == 0.0
    assert len(y) == pytest.approx(N / (1.0 + PPM * 1e-6), abs=2)


def test_block_size_invariant() -> None:
    """Streaming in blocks equals one large call."""
    a = DopplerChannel(
        fs=FS, carrier_hz=FC, doppler_ppm=PPM, doppler_rate_ppm_s=RATE_PPM_S
    )
    b = DopplerChannel(
        fs=FS, carrier_hz=FC, doppler_ppm=PPM, doppler_rate_ppm_s=RATE_PPM_S
    )
    x = _dc()
    ya = a.execute(x)
    yb = np.concatenate(
        [b.execute(x[i : i + 4096]) for i in range(0, len(x), 4096)]
    )
    assert len(ya) == len(yb)
    np.testing.assert_allclose(ya, yb, atol=1e-4)


def test_elapsed_tracks_output_clock() -> None:
    """elapsed_s is receive time produced, and reset() rewinds it."""
    ch = DopplerChannel(fs=FS, carrier_hz=FC, doppler_ppm=PPM)
    y = ch.execute(_dc())
    assert ch.elapsed_s == pytest.approx(len(y) / FS, rel=1e-12)
    ch.reset()
    assert ch.elapsed_s == 0.0


def test_state_round_trip_resumes_bit_exact() -> None:
    """A restored channel produces identical samples from the split point."""
    a = DopplerChannel(
        fs=FS, carrier_hz=FC, doppler_ppm=PPM, doppler_rate_ppm_s=RATE_PPM_S
    )
    b = DopplerChannel(
        fs=FS, carrier_hz=FC, doppler_ppm=PPM, doppler_rate_ppm_s=RATE_PPM_S
    )
    x = _dc(8192)
    a.execute(x)
    b.set_state(a.get_state())
    np.testing.assert_array_equal(a.execute(x), b.execute(x))


def test_state_rejects_bad_blobs() -> None:
    """Wrong size, clobbered envelope, and non-bytes are all rejected."""
    ch = DopplerChannel(fs=FS, carrier_hz=FC, doppler_ppm=PPM)
    blob = ch.get_state()
    assert len(blob) == ch.state_bytes()
    with pytest.raises(ValueError):
        ch.set_state(blob[:-1])
    with pytest.raises(ValueError):
        ch.set_state(bytes([blob[0] ^ 0xFF]) + blob[1:])
    with pytest.raises(TypeError):
        ch.set_state("not bytes")  # type: ignore[arg-type]


@pytest.mark.parametrize("fs", [0.0, -1.0])
def test_invalid_sample_rate_rejected(fs: float) -> None:
    with pytest.raises((ValueError, MemoryError)):
        DopplerChannel(fs=fs, carrier_hz=FC, doppler_ppm=PPM)


def test_time_reversing_doppler_rejected() -> None:
    """d <= -1 would stop or reverse time; refuse rather than emit garbage."""
    with pytest.raises((ValueError, MemoryError)):
        DopplerChannel(fs=FS, carrier_hz=FC, doppler_ppm=-1.0e6)
