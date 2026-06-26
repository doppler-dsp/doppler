"""Theoretical-correctness tests for the Costas carrier loop.

Validates the decision-directed BPSK phase discriminator against closed form:
  * the S-curve e(φ) = sign(cos φ)·sin φ (slope Kd = 1 at lock, nulls at ±90°,
    180° data ambiguity);
  * the discriminator (phase-error) variance vs SNR follows the squaring-loss
    law in the high-SNR regime.
"""

import numpy as np
import pytest

from doppler.track import Costas


def _discriminator(phi):
    c = Costas(bn=1e-6, zeta=0.707, init_norm_freq=0.0, tsamps=1, bn_fll=0.0)
    c.steps(np.array([np.exp(1j * phi)], np.complex64))
    return c.last_error


def test_scurve_matches_theory():
    phis = np.linspace(-np.pi, np.pi, 181)
    meas = np.array([_discriminator(p) for p in phis])
    theory = np.where(np.cos(phis) >= 0, np.sin(phis), -np.sin(phis))
    assert np.max(np.abs(meas - theory)) < 1e-6


def test_scurve_slope_kd_is_unity():
    h = 1e-3
    kd = (_discriminator(h) - _discriminator(-h)) / (2 * h)
    assert kd == pytest.approx(1.0, abs=1e-3)


def test_scurve_stable_lock_and_ambiguity_zeros():
    # zero at φ=0 (stable) and φ=±180° (the BPSK 180° data ambiguity)
    assert _discriminator(0.0) == pytest.approx(0.0, abs=1e-6)
    assert _discriminator(np.pi) == pytest.approx(0.0, abs=1e-6)
    # positive restoring slope through the φ=0 lock
    assert _discriminator(0.1) > 0 and _discriminator(-0.1) < 0


def _disc_var(snr_db, n=120000, seed=1):
    rng = np.random.default_rng(seed)
    snr = 10 ** (snr_db / 10)
    sigma = np.sqrt(1.0 / snr)
    d = rng.integers(0, 2, n) * 2 - 1
    P = (
        d
        + rng.normal(0, sigma / np.sqrt(2), n)
        + 1j * rng.normal(0, sigma / np.sqrt(2), n)
    ).astype(np.complex64)
    c = Costas(bn=1e-7, zeta=0.707, init_norm_freq=0.0, tsamps=1, bn_fll=0.0)
    e = np.empty(n)
    for k in range(n):
        c.steps(P[k : k + 1])
        e[k] = c.last_error
    return np.var(e)


@pytest.mark.parametrize("snr_db", [20, 15, 10])
def test_phase_variance_matches_squaring_loss_at_high_snr(snr_db):
    snr = 10 ** (snr_db / 10)
    law = (1 / (2 * snr)) * (1 + 1 / (2 * snr))
    meas = _disc_var(snr_db)
    assert meas == pytest.approx(law, rel=0.12)


def test_phase_variance_is_bounded_below_law_at_low_snr():
    # the |P|-normalised discriminator saturates where the law diverges
    snr = 10 ** (0 / 10)
    law = (1 / (2 * snr)) * (1 + 1 / (2 * snr))
    assert _disc_var(0) < law
