"""Integration tests for doppler.track.Costas (carrier-tracking loop)."""

import numpy as np
import pytest

from doppler.track import Costas

TSAMPS = 16


def _bpsk_with_carrier(nsym, f0, seed=0, sigma=0.0):
    """Continuous BPSK at the symbol rate with a carrier residual f0."""
    rng = np.random.default_rng(seed)
    bits = rng.integers(0, 2, nsym) * 2 - 1
    sig = np.repeat(bits.astype(np.complex64), TSAMPS)
    k = np.arange(len(sig))
    rx = sig * np.exp(2j * np.pi * f0 * k)
    if sigma:
        rx = rx + (
            rng.normal(0, sigma, len(rx)) + 1j * rng.normal(0, sigma, len(rx))
        )
    return rx.astype(np.complex64), bits


def test_create():
    obj = Costas(0.05, 0.707, 0.0, 64)
    assert obj is not None


def test_context_manager_and_destroy():
    with Costas(0.05, 0.707, 0.0, 64):
        pass
    obj = Costas(0.05, 0.707, 0.0, 64)
    obj.destroy()


def test_properties():
    c = Costas(bn=0.05, zeta=0.707, init_norm_freq=0.01, tsamps=TSAMPS)
    assert c.bn == pytest.approx(0.05)
    assert c.norm_freq == pytest.approx(0.01)
    c.bn = 0.03
    assert c.bn == pytest.approx(0.03)
    assert c.bn_fll == pytest.approx(0.0)
    c.bn_fll = 0.02
    assert c.bn_fll == pytest.approx(0.02)


def test_one_prompt_per_symbol():
    rx, _ = _bpsk_with_carrier(500, 0.0)
    c = Costas(0.05, 0.707, 0.0, TSAMPS)
    sym = c.steps(rx)
    assert sym.dtype == np.complex64
    assert len(sym) == 500


@pytest.mark.parametrize("f0", [0.0, 0.001, 0.003, -0.004])
def test_pull_in_locks_residual(f0):
    rx, bits = _bpsk_with_carrier(4000, f0, seed=1)
    c = Costas(0.05, 0.707, 0.0, TSAMPS)
    sym = c.steps(rx)
    # tracked the residual frequency
    assert c.norm_freq == pytest.approx(f0, abs=2e-4)
    assert c.lock_metric > 0.9
    # ambiguity-tolerant: zero bit errors on the converged tail up to a flip
    tail = len(sym) // 2
    dec = np.where(sym[tail:].real >= 0, 1, -1)
    err = int(np.sum(dec != bits[tail:]))
    assert min(err, len(dec) - err) == 0


def test_reset_reproducible():
    rx, _ = _bpsk_with_carrier(1500, 0.002, seed=3)
    c = Costas(0.05, 0.707, 0.0, TSAMPS)
    s1 = c.steps(rx)
    f1, lk1 = c.norm_freq, c.lock_metric
    c.reset()
    s2 = c.steps(rx)
    assert np.array_equal(s1, s2)
    assert (f1, lk1) == (c.norm_freq, c.lock_metric)


def test_fll_assist_widens_pull_in():
    # ~0.9 rad/symbol residual: too large for the bare PLL at any bandwidth,
    # acquired once the FLL assist is enabled.
    rx, _ = _bpsk_with_carrier(6000, 0.009, seed=5)

    pll = Costas(0.01, 0.707, 0.0, TSAMPS, 0.0)
    pll.steps(rx)
    assert pll.lock_metric < 0.8  # bare PLL fails to acquire it
    assert abs(pll.norm_freq - 0.009) > 1e-3

    fll = Costas(0.01, 0.707, 0.0, TSAMPS, bn_fll=0.03)
    fll.steps(rx)
    assert fll.norm_freq == pytest.approx(0.009, abs=5e-4)
    assert fll.lock_metric > 0.9
    assert fll.bn_fll == pytest.approx(0.03)


def test_locks_under_noise():
    rx, bits = _bpsk_with_carrier(5000, 0.0015, seed=7, sigma=1.0)
    c = Costas(0.03, 0.707, 0.0, TSAMPS)
    sym = c.steps(rx)
    assert c.norm_freq == pytest.approx(0.0015, abs=5e-4)
    assert c.lock_metric > 0.7
    tail = len(sym) // 2
    dec = np.where(sym[tail:].real >= 0, 1, -1)
    err = int(np.sum(dec != bits[tail:]))
    assert min(err, len(dec) - err) == 0
