"""Tests for the feedforward polynomial-phase estimator (ppe).

A coherent (chirp-rate x frequency) matched-filter surface recovers the Doppler
and Doppler rate of a complex sequence in one shot (no loop). The ``max_rate``
knob spans the two regimes: 0 = a single FFT (near-static Doppler), > 0 = a
dechirp-bank search (severe LEO chirp). These drive synthetic chirps — clean,
noisy, and edge cases — asserting the estimate tracks the injection.
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


def test_create_doppler_only():
    """max_rate = 0 collapses the rate axis to a single FFT."""
    p = PolyPhaseEstimator(4096, 0.0)
    assert p.max_len == 4096 and p.nfft == 4096
    assert p.max_rate == 0.0 and p.n_rate == 1


def test_create_rate_search_grid():
    """max_rate > 0 builds an odd, multi-row chirp-rate grid."""
    p = PolyPhaseEstimator(512, 5e-5)
    assert p.max_rate == 5e-5
    assert p.n_rate > 1 and p.n_rate % 2 == 1  # r = 0 is a grid node


def test_doppler_only_recovers_tone():
    """Pure Doppler: a tone recovers, rate is forced to exactly 0."""
    n = 2048
    p = PolyPhaseEstimator(n, 0.0)
    e = p.estimate(_chirp(n, 0.1, 0.0))
    assert abs(e.freq_norm - 0.1) <= 5e-3
    assert e.rate_norm == 0.0


@pytest.mark.parametrize("f,r", [(0.05, 1e-5), (-0.08, -2e-5), (0.12, 0.0)])
def test_joint_search_recovers_chirp(f, r):
    """Coherent surface recovers both frequency and chirp rate."""
    n = 512
    p = PolyPhaseEstimator(n, 5e-5)
    e = p.estimate(_chirp(n, f, r))
    assert abs(e.freq_norm - f) <= 5e-3
    assert abs(e.rate_norm - r) <= 5e-6


def test_record_fields():
    e = PolyPhaseEstimator(512, 5e-5).estimate(_chirp(512, 0.05, 1e-5))
    assert hasattr(e, "freq_norm") and hasattr(e, "rate_norm")
    assert e.snr_db > 20.0  # clean chirp concentrates far above the floor


def test_robust_under_noise():
    """At ~10 dB SNR the coherent surface still tracks the injected chirp."""
    n, f, r = 512, 0.07, 1.5e-5
    p = PolyPhaseEstimator(n, 5e-5)
    sigma = 10 ** (-10.0 / 20.0)  # 10 dB SNR (unit-power signal)
    errs_f, errs_r = [], []
    for seed in range(8):
        rng = np.random.default_rng(seed)
        e = p.estimate(_chirp(n, f, r, rng=rng, sigma=sigma))
        errs_f.append(abs(e.freq_norm - f))
        errs_r.append(abs(e.rate_norm - r))
    assert np.median(errs_f) <= 5e-3
    assert np.median(errs_r) <= 5e-6


def test_out_of_range_length_is_zeroed():
    p = PolyPhaseEstimator(512, 0.0)
    e = p.estimate(_chirp(1024, 0.05, 0.0))  # longer than max_len
    assert e.freq_norm == 0.0 and e.rate_norm == 0.0
