"""Tests for doppler.ddc.DDCR."""

from __future__ import annotations

import numpy as np
import pytest

from doppler.ddc import DDCR

N = 4096


def _real_tone(freq: float, n: int) -> np.ndarray:
    t = np.arange(n, dtype=np.float64)
    return np.cos(2 * np.pi * freq * t).astype(np.float32)


def _dominant_freq(y: np.ndarray) -> float:
    S = np.abs(np.fft.fft(y))
    return float(np.fft.fftfreq(len(y))[np.argmax(S)])


# ------------------------------------------------------------------ #
# Construction                                                         #
# ------------------------------------------------------------------ #


class TestDdcRConstruction:
    def test_create_basic(self):
        r = DDCR(0.0, 0.25)
        assert r is not None

    def test_invalid_rate_zero(self):
        with pytest.raises((ValueError, Exception)):
            DDCR(0.0, 0.0)

    def test_invalid_rate_ge_half(self):
        with pytest.raises((ValueError, Exception)):
            DDCR(0.0, 0.5)

    def test_rate_property(self):
        r = DDCR(0.0, 0.25)
        assert abs(r.rate - 0.25) < 1e-9

    def test_get_norm_freq(self):
        r = DDCR(0.1, 0.25)
        assert abs(r.get_norm_freq() - 0.1) < 1e-5

    def test_context_manager(self):
        with DDCR(0.0, 0.25) as r:
            y = r.execute(np.zeros(512, dtype=np.float32))
        assert y.dtype == np.complex64


# ------------------------------------------------------------------ #
# execute()                                                            #
# ------------------------------------------------------------------ #


class TestDdcRExecute:
    def test_returns_complex64(self):
        r = DDCR(0.0, 0.25)
        y = r.execute(np.zeros(N, dtype=np.float32))
        assert y.dtype == np.complex64

    def test_output_length_approx(self):
        r = DDCR(0.0, 0.25)
        x = np.ones(N, dtype=np.float32)
        y = r.execute(x)
        # Total rate 0.25 → output ≈ N * 0.25
        expected = N * 0.25
        assert abs(len(y) / expected - 1.0) < 0.1, (
            f"got {len(y)}, expected ≈{expected:.0f}"
        )

    def test_rejects_wrong_dtype(self):
        r = DDCR(0.0, 0.25)
        with pytest.raises((TypeError, ValueError)):
            r.execute(np.zeros(N, dtype=np.complex64))


# ------------------------------------------------------------------ #
# Tuning and reset                                                     #
# ------------------------------------------------------------------ #


class TestDdcRTuning:
    def test_set_norm_freq_roundtrip(self):
        r = DDCR(0.1, 0.25)
        r.set_norm_freq(0.2)
        assert abs(r.get_norm_freq() - 0.2) < 1e-5

    def test_reset_gives_same_output(self):
        rng = np.random.default_rng(13)
        x = rng.standard_normal(N).astype(np.float32)
        r1 = DDCR(0.0, 0.25)
        r2 = DDCR(0.0, 0.25)
        out1 = r1.execute(x)
        r2.execute(x)
        r2.reset()
        out2 = r2.execute(x)
        np.testing.assert_array_equal(out1, out2)


# ------------------------------------------------------------------ #
# Spectral: real tone at f_carrier tuned to DC                        #
# ------------------------------------------------------------------ #


def test_real_tone_to_dc():
    """Real tone at 0.1 fs_in appears near DC after DDCR chain.

    With the embedded fs/4 shift in the halfband R2C, a real cosine at
    f_carrier (input rate) appears at the intermediate rate at
    f_int = 2*(f_carrier + 0.25) = 2*f_carrier + 0.5.

    To shift to DC at the intermediate stage, the NCO should be tuned
    to norm_freq = -(2*f_carrier + 0.5).
    """
    f_carrier = 0.1
    norm_freq = -(2 * f_carrier + 0.5)
    r = DDCR(norm_freq, 0.25)
    x = _real_tone(f_carrier, N * 8)
    y = r.execute(x)
    if len(y) < 16:
        pytest.skip("output too short for spectral check")
    dominant = _dominant_freq(y)
    assert abs(dominant) < 0.05, f"dominant at {dominant:.4f}, expected near 0"
