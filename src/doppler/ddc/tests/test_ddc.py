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
        assert abs(ddc.get_norm_freq() - 0.1) < 1e-5

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
        ddc.set_norm_freq(0.2)
        assert abs(ddc.get_norm_freq() - 0.2) < 1e-5

    def test_set_norm_freq_no_reset(self):
        ddc = DDC(0.1, 0.25)
        ddc.execute(np.ones(512, dtype=np.complex64))
        ddc.set_norm_freq(0.2)
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
