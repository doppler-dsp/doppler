"""Tests for doppler.ddc.DDC."""

from __future__ import annotations

import numpy as np
import pytest

from doppler.ddc import DDC

N = 4096


def _tone(freq: float, n: int, offset: int = 0) -> np.ndarray:
    t = np.arange(n, dtype=np.float64) + offset
    return np.exp(2j * np.pi * freq * t).astype(np.complex64)


def _dominant_freq(y: np.ndarray) -> float:
    S = np.abs(np.fft.fft(y))
    return float(np.fft.fftfreq(len(y))[np.argmax(S)])


# ------------------------------------------------------------------ #
# Construction                                                         #
# ------------------------------------------------------------------ #


class TestDdcConstruction:
    def test_create_basic(self):
        ddc = DDC(0.1, 0.25)
        assert ddc is not None

    def test_invalid_rate_zero(self):
        with pytest.raises((ValueError, Exception)):
            DDC(0.1, 0.0)

    def test_invalid_rate_negative(self):
        with pytest.raises((ValueError, Exception)):
            DDC(0.1, -1.0)

    def test_rate_property(self):
        ddc = DDC(0.1, 0.25)
        assert abs(ddc.rate - 0.25) < 1e-9

    def test_get_norm_freq(self):
        ddc = DDC(0.1, 0.25)
        assert abs(ddc.norm_freq - 0.1) < 1e-5

    def test_context_manager(self):
        with DDC(0.1, 0.25) as ddc:
            y = ddc.execute(np.zeros(512, dtype=np.complex64))
        assert y.dtype == np.complex64


# ------------------------------------------------------------------ #
# execute() output                                                     #
# ------------------------------------------------------------------ #


class TestDdcExecute:
    def test_returns_complex64(self):
        ddc = DDC(0.1, 0.25)
        y = ddc.execute(np.zeros(N, dtype=np.complex64))
        assert y.dtype == np.complex64

    @pytest.mark.parametrize("rate,tol", [(0.25, 0.05), (0.5, 0.05)])
    def test_output_length_approx(self, rate, tol):
        ddc = DDC(0.1, rate)
        x = np.ones(N, dtype=np.complex64)
        y = ddc.execute(x)
        expected = N * rate
        assert abs(len(y) / expected - 1.0) < tol, (
            f"rate={rate}: got {len(y)}, expected ≈{expected:.0f}"
        )

    def test_unity_rate_length(self):
        ddc = DDC(0.0, 1.0)
        x = np.ones(512, dtype=np.complex64)
        y = ddc.execute(x)
        assert len(y) == 512


# ------------------------------------------------------------------ #
# Frequency tuning                                                     #
# ------------------------------------------------------------------ #


class TestDdcTuning:
    def test_set_norm_freq_roundtrip(self):
        ddc = DDC(0.1, 0.25)
        ddc.norm_freq = 0.2
        assert abs(ddc.norm_freq - 0.2) < 1e-5

    def test_set_norm_freq_no_reset(self):
        ddc = DDC(0.1, 0.25)
        ddc.execute(np.ones(512, dtype=np.complex64))
        ddc.norm_freq = 0.2
        y = ddc.execute(np.ones(512, dtype=np.complex64))
        assert len(y) > 0


# ------------------------------------------------------------------ #
# reset()                                                              #
# ------------------------------------------------------------------ #


class TestDdcReset:
    def test_reset_gives_same_output_as_fresh(self):
        rng = np.random.default_rng(7)
        x = rng.standard_normal(N).astype(np.complex64)
        d1 = DDC(0.1, 0.25)
        d2 = DDC(0.1, 0.25)
        out1 = d1.execute(x)
        d2.execute(x)
        d2.reset()
        out2 = d2.execute(x)
        np.testing.assert_array_equal(out1, out2)

    def test_reset_preserves_rate(self):
        ddc = DDC(0.1, 0.25)
        ddc.reset()
        assert abs(ddc.rate - 0.25) < 1e-9


# ------------------------------------------------------------------ #
# Spectral                                                             #
# ------------------------------------------------------------------ #


@pytest.mark.parametrize("f_tone", [0.1, -0.1, 0.2])
def test_tone_shifted_to_dc(f_tone):
    """A tone at f_tone is brought to DC by DDC(norm_freq=-f_tone, ...)."""
    ddc = DDC(-f_tone, 0.25)
    offset = 0
    y_last = None
    for _ in range(4):
        x = _tone(f_tone, N, offset)
        y_last = ddc.execute(x)
        offset += N
    dominant = _dominant_freq(y_last)
    assert abs(dominant) < 0.02, (
        f"f_tone={f_tone}: dominant at {dominant:.4f}, expected near 0"
    )


def test_ddc_execute_result_survives_buffer_grow():
    # gh-219 regression: holding an execute() result across a larger execute()
    # (which grows the internal buffer) must not dangle. The output is a fresh
    # numpy-owned array per call, not a view of a realloc'd internal buffer.
    d = DDC(0.1, 0.25)
    rng = np.random.default_rng(0)
    y1 = d.execute(
        (rng.standard_normal(64) + 1j * rng.standard_normal(64)).astype(
            np.complex64
        )
    )
    snapshot = y1.copy()
    big = d.execute(
        (rng.standard_normal(8192) + 1j * rng.standard_normal(8192)).astype(
            np.complex64
        )
    )
    assert y1.ctypes.data != big.ctypes.data  # independent buffers
    assert np.array_equal(y1, snapshot)  # no use-after-free


def test_ddc_state_roundtrip_resume():
    """The serializable (elastic) face: serialize the complex DDC mid-stream,
    restore into a fresh DDC from the same (norm_freq, rate), and resume — the
    continuation matches an uninterrupted run bit-for-bit; a wrong-size or
    clobbered blob is rejected."""
    rng = np.random.default_rng(7)
    x = (rng.standard_normal(2400) + 1j * rng.standard_normal(2400)).astype(
        np.complex64
    )
    cut = 900

    ref = DDC(-0.1, 0.25)
    ref.execute(x[:cut])
    tail_ref = ref.execute(x[cut:])

    a = DDC(-0.1, 0.25)
    a.execute(x[:cut])
    blob = a.get_state()
    assert isinstance(blob, bytes) and len(blob) == a.state_bytes()

    b = DDC(-0.1, 0.25)
    b.set_state(blob)
    assert np.array_equal(b.execute(x[cut:]), tail_ref)

    with pytest.raises(ValueError):  # size mismatch
        b.set_state(blob[:-1])
    with pytest.raises(ValueError):  # clobbered envelope magic
        b.set_state(bytes([blob[0] ^ 0xFF]) + blob[1:])
