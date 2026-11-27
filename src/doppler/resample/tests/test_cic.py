"""Signal-level tests for doppler.resample.CIC.

CIC is fixed at N=4 stages, M=1, power-of-two R.

Covers lifecycle, decimation mechanics, and spectral quality.
Frequency-domain checks use a Blackman-Harris windowed FFT so spurious
lobes do not mask real failures.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.resample import CIC

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _blackman_harris(n: int) -> np.ndarray:
    a = [0.35875, 0.48829, 0.14128, 0.01168]
    k = 2 * np.pi * np.arange(n) / n
    return (
        a[0] - a[1] * np.cos(k) + a[2] * np.cos(2 * k) - a[3] * np.cos(3 * k)
    )


def _power_db(signal: np.ndarray, freq_norm: float, pad: int = 4) -> float:
    """Return dBFS power of the component nearest freq_norm (0..1)."""
    n = len(signal)
    w = _blackman_harris(n)
    cg = w.mean()
    S = np.fft.fft(signal * w, n * pad)
    bins = np.fft.fftfreq(n * pad)
    idx = int(np.argmin(np.abs(bins - freq_norm)))
    peak = float(np.abs(S[idx])) / (n * cg)
    return 20.0 * np.log10(peak + 1e-300)


def _tone(freq_norm: float, n: int, dtype=np.complex64) -> np.ndarray:
    """Complex exponential at freq_norm (cycles/sample, 0..1)."""
    t = np.arange(n)
    return np.exp(2j * np.pi * freq_norm * t).astype(dtype)


def _decimate(cic: CIC, x: np.ndarray, block: int = 0) -> np.ndarray:
    """Feed x through cic in blocks, return concatenated output."""
    if block == 0:
        return np.array(cic.decimate(x), copy=True)
    chunks = []
    for i in range(0, len(x), block):
        out = cic.decimate(x[i : i + block])
        if len(out):
            chunks.append(np.array(out, copy=True))
    return (
        np.concatenate(chunks) if chunks else np.array([], dtype=np.complex64)
    )


# ---------------------------------------------------------------------------
# Lifecycle
# ---------------------------------------------------------------------------


class TestLifecycle:
    def test_create_default(self):
        obj = CIC()
        assert obj.R == 16
        assert obj.shift == 16  # 4 * log2(16)

    def test_create_explicit(self):
        obj = CIC(8)
        assert obj.R == 8
        assert obj.shift == 12  # 4 * log2(8)

    def test_invalid_r_non_power_of_two(self):
        with pytest.raises((ValueError, MemoryError)):
            CIC(3)

    def test_invalid_r_zero(self):
        with pytest.raises((ValueError, MemoryError)):
            CIC(0)

    def test_invalid_r_one(self):
        with pytest.raises((ValueError, MemoryError)):
            CIC(1)

    def test_context_manager(self):
        with CIC(4) as obj:
            assert obj.R == 4

    def test_destroy(self):
        obj = CIC(4)
        obj.destroy()

    def test_reset_reproducible(self):
        R, n_in = 8, 8 * 8 * 4
        x = _tone(0.01, n_in)
        obj = CIC(R)
        out1 = np.array(obj.decimate(x), copy=True)
        obj.reset()
        out2 = np.array(obj.decimate(x), copy=True)
        np.testing.assert_array_equal(out1, out2)

    def test_reconfigure(self):
        obj = CIC(4)
        obj.reconfigure(32)
        assert obj.R == 32
        assert obj.shift == 20  # 4 * log2(32)

    def test_reconfigure_invalid_ignored(self):
        obj = CIC(8)
        obj.reconfigure(3)  # non-power-of-two — silently ignored
        assert obj.R == 8


# ---------------------------------------------------------------------------
# Decimation mechanics
# ---------------------------------------------------------------------------


class TestDecimation:
    def test_output_count(self):
        R = 8
        obj = CIC(R)
        x = np.zeros(4 * R, dtype=np.complex64)
        assert len(obj.decimate(x)) == 4

    def test_partial_block_accumulates(self):
        """R-1 samples produce 0 outputs; the R-th completes the cycle."""
        R = 8
        obj = CIC(R)
        x = np.zeros(R, dtype=np.complex64)
        assert len(obj.decimate(x[: R - 1])) == 0
        assert len(obj.decimate(x[R - 1 :])) == 1

    def test_zero_input(self):
        # Offset-binary encoding maps 0.0 → u=32768, so the first CIC_N=4
        # output periods are transient.  From index 4 onward, output is
        # exactly 0+0j.
        CIC_N = 4
        R = 4
        obj = CIC(R)
        x = np.zeros(64, dtype=np.complex64)
        out = obj.decimate(x)
        np.testing.assert_array_equal(out[CIC_N:], 0)

    def test_streaming_continuity(self):
        """Splitting input at an arbitrary boundary gives identical output."""
        R, n_in = 16, 8 * 16
        x = _tone(0.02, n_in)
        out_whole = _decimate(CIC(R), x)
        out_split = _decimate(CIC(R), x, block=R)
        np.testing.assert_array_almost_equal(out_whole, out_split, decimal=5)

    def test_dc_passthrough(self):
        """Settled DC output must equal 1.0 within Q15 tolerance."""
        R, N = 8, 4
        n_in = 12 * R * N
        x = np.ones(n_in, dtype=np.complex64)
        obj = CIC(R)
        out = obj.decimate(x)
        settled = out[len(out) * 3 // 4 :]
        np.testing.assert_allclose(np.abs(settled), 1.0, atol=4e-5)


# ---------------------------------------------------------------------------
# Spectral quality
# ---------------------------------------------------------------------------


class TestSpectralQuality:
    """Verify passband gain and alias rejection via FFT."""

    R = 8
    N = 4  # fixed — kept as class attr for readability
    F_PASS = 0.5 / (2 * R) * 0.10  # deep in passband (~0.003 of input fs)
    F_ALIAS = 1.0 / R * 0.95  # just inside first null

    def test_passband_gain(self):
        """Single passband tone at unit amplitude survives with < 3 dB loss."""
        n_transient = self.N * (self.R - 1)
        n_in = (n_transient + 512) * self.R
        x = _tone(self.F_PASS, n_in)
        y = _decimate(CIC(self.R), x)
        n_drop = n_transient // self.R + 1
        gain_db = _power_db(y[n_drop:], self.F_PASS * self.R)
        assert gain_db > -3.0, f"Passband attenuated {-gain_db:.1f} dB"

    def test_alias_rejection(self):
        """Alias-zone tone is at least 20 dB below passband tone."""
        n_transient = self.N * (self.R - 1)
        n_in = (n_transient + 512) * self.R
        # 0.5 amplitude keeps two-tone sum within UQ16 range
        x = 0.5 * (_tone(self.F_PASS, n_in) + _tone(self.F_ALIAS, n_in))
        y = _decimate(CIC(self.R), x)
        n_drop = n_transient // self.R + 1
        y = y[n_drop:]
        f_alias_out = (self.F_ALIAS % (1.0 / self.R)) * self.R
        alias_db = _power_db(y, f_alias_out)
        passband_db = _power_db(y, self.F_PASS * self.R)
        rejection = passband_db - alias_db
        assert rejection >= 20.0, f"Alias rejection only {rejection:.1f} dB"

    def test_stopband_null(self):
        """Tone at exactly fs/R (first CIC null) is near-zero after
        settling."""
        R, N = 8, 4
        F_NULL = 1.0 / R
        x = _tone(F_NULL, 32 * R * N)
        obj = CIC(R)
        y = _decimate(obj, x)
        n_drop = N * (R - 1) // R + 4
        rms = float(np.sqrt(np.mean(np.abs(y[n_drop:]) ** 2)))
        assert rms < 1e-3, f"Null tone RMS {rms:.2e} (expected < 1e-3)"


# ------------------------------------------------------------------ #
# Input amplitude bound                                               #
# ------------------------------------------------------------------ #


# The encoder reserves PAPR headroom above unit amplitude, so full scale is
# this, not 1.0 -- see docs/design/cic.md. A pulse-shaped stream peaks well
# above its symbol amplitude (1.582x for RRC), and budgeting that here is what
# lets a caller present a signal at its natural level.
_CIC_HEADROOM = CIC(R=16).full_scale  # the bound as built, 4.0


def test_input_beyond_full_scale_is_clipped_silently():
    # The CIC is the one block in doppler that is NOT scale-free: its input is
    # bounded and anything past that is clipped before filtering. No
    # exception, no NaN -- just a plausible-looking wrong answer, which is
    # exactly why it is documented so loudly.
    cic = CIC(R=16)
    for amp in (0.5, 1.0, _CIC_HEADROOM):
        y = CIC(R=16).decimate(np.full(256, amp, dtype=np.complex64))
        assert y[-1].real == pytest.approx(amp, rel=1e-3)
    for amp in (10.0, 1e6):
        y = CIC(R=16).decimate(np.full(256, amp, dtype=np.complex64))
        assert np.isfinite(y[-1].real)
        assert y[-1].real == pytest.approx(_CIC_HEADROOM, rel=1e-3)
    # Clipping is per component and symmetric.
    y = cic.decimate(np.full(256, -5.0 + 5.0j, dtype=np.complex64))
    assert y[-1].real == pytest.approx(-_CIC_HEADROOM, rel=1e-3)
    assert y[-1].imag == pytest.approx(_CIC_HEADROOM, rel=1e-3)


def test_clipped_flag_is_sticky_and_free():
    # The flag is the only reliable signal, since the sample stream gives
    # none. It costs nothing: the boundary comparisons run regardless.
    c = CIC(R=16)
    assert c.clipped is False
    c.decimate(np.full(256, 0.9, dtype=np.complex64))
    assert c.clipped is False
    c.decimate(np.full(256, 1.5, dtype=np.complex64))
    assert c.clipped is False  # inside the PAPR headroom
    c.decimate(np.full(256, 1.05 * _CIC_HEADROOM, dtype=np.complex64))
    assert c.clipped is True
    # Sticky: a later in-range block does not clear it...
    c.decimate(np.full(256, 0.1, dtype=np.complex64))
    assert c.clipped is True
    # ...only reset() does.
    c.reset()
    assert c.clipped is False


# Past the bound, not exactly on it: the Q15 range is asymmetric by one LSB
# (-32768 is representable, +32768 is not), so a probe sitting exactly at
# +/-_CIC_HEADROOM clips on the positive rail only.
@pytest.mark.parametrize(
    "bad",
    [
        1.05 * _CIC_HEADROOM + 0j,
        -1.05 * _CIC_HEADROOM + 0j,
        0 + 1.05 * _CIC_HEADROOM * 1j,
        0 - 1.05 * _CIC_HEADROOM * 1j,
    ],
)
def test_clipped_catches_every_component_and_sign(bad):
    c = CIC(R=16)
    c.decimate(np.full(256, bad, dtype=np.complex64))
    assert c.clipped is True


def test_clipped_survives_state_round_trip():
    # `clipped` is running state, not a side-channel diagnostic: a resumed
    # stream that forgot it had clipped would answer the question wrongly.
    a = CIC(R=16)
    a.decimate(np.full(256, 1.5 * a.full_scale, dtype=np.complex64))
    assert a.clipped is True
    b = CIC(R=16)
    assert b.clipped is False
    b.set_state(a.get_state())
    assert b.clipped is True


def test_state_carries_r_from_reconfigure():
    """R travels in the blob, so a restore follows reconfigure() (#2642).

    A blob from R=64 restored into an R=16 CIC used to be accepted and its
    accumulators read at the R=16 gain; now the target takes R=64 and its
    continuation matches the source's exactly.
    """
    rng = np.random.default_rng(7)
    x = 0.3 * (rng.standard_normal(2048) + 1j * rng.standard_normal(2048))
    x = x.astype(np.complex64)
    src = CIC(R=16)
    src.reconfigure(64)
    src.decimate(x[:333])
    blob = src.get_state()
    want = src.decimate(x[333:])

    dst = CIC(R=16)
    dst.set_state(blob)
    assert dst.R == 64
    assert dst.shift == src.shift
    np.testing.assert_array_equal(dst.decimate(x[333:]), want)


# ------------------------------------------------------------------ #
# The input word: precision, headroom, step                            #
# ------------------------------------------------------------------ #


def test_a_cic_is_built_with_the_widest_word():
    # 64: all the register has left after the pipeline's 4*log2(R) bits of
    # gain, at full scale 4.0. The C test (test_the_word) holds the word's
    # arithmetic; this is the binding.
    c = CIC(R=64)
    assert (c.precision, c.bits, c.headroom_bits) == (64, 40, 2)
    assert c.full_scale == 4.0
    assert c.step == 4.0 / 2**39
    c.precision = 32
    assert (c.bits, c.full_scale, c.step) == (32, 4.0, 4.0 / 2**31)
    c.precision = 16  # the filter as it was: a 16-bit word at 2.0
    assert (c.bits, c.headroom_bits, c.full_scale) == (16, 1, 2.0)
    assert c.step == 2.0 / 2**15
    # past R = 256 the register has under 32 bits left
    assert CIC(R=1024).bits == 24


def test_headroom_trades_range_for_step():
    c = CIC(R=64)
    step = c.step
    c.headroom_bits = 4
    assert c.full_scale == 16.0 and c.step == 4 * step
    c.decimate(np.full(256, 10.0, dtype=np.complex64))
    assert c.clipped is False  # 10 of 16
    c.precision = 64  # a precision puts its own headroom back
    assert c.headroom_bits == 2 and c.full_scale == 4.0
    c.decimate(np.full(256, 10.0, dtype=np.complex64))
    assert c.clipped is True  # 10 of 4


@pytest.mark.parametrize("bad", [0, 8, 24, 33, 128])
def test_a_precision_no_filter_has_is_refused(bad):
    c = CIC(R=16)
    with pytest.raises(ValueError, match="precision is 16, 32 or 64"):
        c.precision = bad
    assert c.precision == 64
    with pytest.raises(ValueError, match="headroom_bits is 0 to 14"):
        c.headroom_bits = 15
    assert c.headroom_bits == 2


def test_the_step_is_the_floor():
    # A tone a sixth of a 16-bit step in amplitude: the 16-bit word reads
    # it as silence, and the default word passes it at its own amplitude.
    amp = 1e-5
    x = (amp * np.exp(2j * np.pi * np.arange(16 * 400) / (512 * 16))).astype(
        np.complex64
    )
    wide, narrow = CIC(R=16), CIC(R=16)
    narrow.precision = 16
    assert amp < narrow.step < 10 * amp
    y16 = np.array(narrow.decimate(x))[8:]
    y64 = np.array(wide.decimate(x))[8:]
    assert np.all(y16 == 0)
    assert np.max(np.abs(np.abs(y64) - amp)) < 1e-3 * amp


def test_settings_reset_the_filter_and_travel_in_the_blob():
    a = CIC(R=16)
    a.decimate(np.full(64, 100.0, dtype=np.complex64))
    assert a.clipped is True
    a.precision = 32  # a new word: the registers hold the old one's samples
    assert a.clipped is False
    a.headroom_bits = 5
    x = (
        np.random.default_rng(4).standard_normal(320)
        + 1j * np.random.default_rng(5).standard_normal(320)
    ).astype(np.complex64)
    a.decimate(x[:173])
    b = CIC(R=16)  # built with nothing set
    b.set_state(a.get_state())
    assert (b.precision, b.headroom_bits, b.full_scale) == (32, 5, 32.0)
    assert (
        np.array(a.decimate(x[173:])).tobytes()
        == np.array(b.decimate(x[173:])).tobytes()
    )
