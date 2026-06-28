"""Tests for the feedforward polynomial-phase estimator (ppe / HAF).

A 2-lag Higher-order Ambiguity Function recovers the frequency and chirp-rate
of a complex sequence in one shot (no loop). These drive synthetic chirps —
clean, noisy, and edge cases — asserting the estimate tracks the injection.
"""

import numpy as np
import pytest

from doppler.dsss import PolyPhaseEstimator


def _chirp(n, f, r, *, rng=None, sigma=0.0):
    """Unit-amplitude linear chirp exp(j2π(f·m + ½·r·m²)) + optional AWGN."""
    m = np.arange(n)
    y = np.exp(2j * np.pi * (f * m + 0.5 * r * m * m))
    if sigma and rng is not None:
        y = y + (sigma / np.sqrt(2.0)) * (
            rng.standard_normal(n) + 1j * rng.standard_normal(n)
        )
    return y.astype(np.complex64)


def test_create():
    p = PolyPhaseEstimator(4096, 0)
    assert p.max_len == 4096
    assert p.nfft == 4096  # already a power of two
    assert p.lag == 0


@pytest.mark.parametrize(
    "f,r",
    [(0.05, 1e-5), (-0.08, -2e-5), (0.1, 0.0), (0.0, 3e-5)],
)
def test_recovers_clean_chirp(f, r):
    """Noise-free: frequency and chirp-rate recover to a fraction of a bin."""
    n = 2048
    p = PolyPhaseEstimator(n, 0)
    e = p.estimate(_chirp(n, f, r))
    assert abs(e.freq_norm - f) <= 5e-3
    assert abs(e.rate_norm - r) <= 5e-6


def test_record_fields():
    e = PolyPhaseEstimator(2048, 0).estimate(_chirp(2048, 0.05, 1e-5))
    assert hasattr(e, "freq_norm") and hasattr(e, "rate_norm")
    assert hasattr(e, "snr_db")
    assert e.snr_db > 20.0  # clean tone is far above the floor


def test_robust_under_noise():
    """At ~10 dB SNR the estimate still tracks the injected chirp."""
    n = 2048
    f, r = 0.07, 1.5e-5
    p = PolyPhaseEstimator(n, 0)
    rng = np.random.default_rng(0)
    sigma = 10 ** (-10.0 / 20.0)  # 10 dB SNR (unit-power signal)
    errs_f, errs_r = [], []
    for seed in range(8):
        rng = np.random.default_rng(seed)
        e = p.estimate(_chirp(n, f, r, rng=rng, sigma=sigma))
        errs_f.append(abs(e.freq_norm - f))
        errs_r.append(abs(e.rate_norm - r))
    # median error stays small even though individual trials see noise.
    assert np.median(errs_f) <= 5e-3
    assert np.median(errs_r) <= 5e-6


def test_out_of_range_length_is_zeroed():
    p = PolyPhaseEstimator(512, 0)
    e = p.estimate(_chirp(1024, 0.05, 1e-5))  # longer than max_len
    assert e.freq_norm == 0.0 and e.rate_norm == 0.0
