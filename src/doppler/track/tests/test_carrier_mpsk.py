"""Integration tests for doppler.track.CarrierMpsk (M-PSK carrier loop).

The M-ary generalization of the Costas loop: per-sample integer-NCO carrier
wipe-off, coherent integrate-and-dump over each symbol, a decision-directed
M-PSK phase discriminator (slice + ``Im(P conj(ahat))/|P|``), an optional
decision-directed FLL assist, and integer-NCO steering. The loop locks to one
of ``m`` phases (an M-fold ambiguity resolved downstream), so symbol scoring
here is ambiguity-tolerant.
"""

import numpy as np
import pytest

from doppler.mpsk import mpsk_map
from doppler.track import CarrierMpsk, Costas

TSAMPS = 16


def _mpsk_with_carrier(m, nsym, f0, seed=0, sigma=0.0):
    """Continuous M-PSK at the symbol rate with a carrier residual ``f0``.

    Returns the cf32 receive buffer and the Gray labels transmitted.
    """
    rng = np.random.default_rng(seed)
    labels = rng.integers(0, m, nsym).astype(np.uint8)
    pts = mpsk_map(labels, m)  # unit-energy constellation points
    sig = np.repeat(pts, TSAMPS).astype(np.complex64)
    k = np.arange(len(sig))
    rx = sig * np.exp(2j * np.pi * f0 * k)
    if sigma:
        rx = rx + (
            rng.normal(0, sigma, len(rx)) + 1j * rng.normal(0, sigma, len(rx))
        )
    return rx.astype(np.complex64), labels


def _symbol_errors(out, labels, m):
    """Ambiguity-tolerant symbol errors over the converged tail.

    The loop locks to one of ``m`` global phases; score the minimum error
    count across the ``m`` de-rotations.
    """
    tail0 = len(out) // 2
    out = out[tail0:]
    truth = labels[tail0 : tail0 + len(out)]
    const = mpsk_map(np.arange(m, dtype=np.uint8), m)  # label -> point
    u = out / (np.abs(out) + 1e-12)
    best = len(out) + 1
    for r in range(m):
        rot = np.exp(-2j * np.pi * r / m)
        # nearest label = argmax Re(u*rot . conj(point))
        proj = np.real((u * rot)[:, None] * np.conj(const)[None, :])
        dec = proj.argmax(axis=1)
        best = min(best, int(np.count_nonzero(dec != truth)))
    return best


# -------------------------------------------------------------------- #
# Lifecycle / API                                                      #
# -------------------------------------------------------------------- #
def test_create_defaults_and_kwargs():
    # all params default + keyword-capable (no forced positionals)
    assert CarrierMpsk() is not None
    c = CarrierMpsk(
        bn=0.05, zeta=0.707, init_norm_freq=0.01, tsamps=TSAMPS, m=4
    )
    assert c.m == 4
    assert c.norm_freq == pytest.approx(0.01)
    assert c.bn == pytest.approx(0.05)


def test_context_manager_and_destroy():
    with CarrierMpsk(m=4):
        pass
    obj = CarrierMpsk(m=8)
    obj.destroy()


@pytest.mark.parametrize("m", [2, 4, 8])
def test_valid_orders(m):
    assert CarrierMpsk(m=m).m == m


@pytest.mark.parametrize("m", [0, 1, 3, 5, 16])
def test_invalid_order_raises(m):
    # carrier_mpsk_create rejects M not in {2,4,8} -> NULL -> MemoryError
    with pytest.raises(MemoryError):
        CarrierMpsk(m=m)


def test_setters():
    c = CarrierMpsk(m=4, tsamps=TSAMPS)
    c.bn = 0.02
    assert c.bn == pytest.approx(0.02)
    c.bn_fll = 0.01
    assert c.bn_fll == pytest.approx(0.01)
    c.norm_freq = 0.003
    assert c.norm_freq == pytest.approx(0.003)


# -------------------------------------------------------------------- #
# The headline anchor: m=2 IS the BPSK Costas loop                     #
# -------------------------------------------------------------------- #
def test_m2_equals_costas():
    rx, _ = _mpsk_with_carrier(2, 2000, 0.002, seed=4242, sigma=0.05)
    cc = Costas(0.05, 0.707, 0.0, TSAMPS, 0.0)
    cm = CarrierMpsk(0.05, 0.707, 0.0, TSAMPS, 0.0, 2)
    oc = cc.steps(rx)
    om = cm.steps(rx)
    assert oc.shape == om.shape
    # identical up to float rounding in the two slicers (sign vs nearest-point)
    assert np.max(np.abs(oc - om)) < 1e-4
    assert cm.norm_freq == pytest.approx(cc.norm_freq, abs=1e-6)
    assert cm.lock_metric == pytest.approx(cc.lock_metric, abs=1e-5)


# -------------------------------------------------------------------- #
# Tracking                                                             #
# -------------------------------------------------------------------- #
@pytest.mark.parametrize("m", [2, 4])
@pytest.mark.parametrize("f0", [0.0, 0.001, -0.0012])
def test_pull_in_pure_pll(m, f0):
    rx, labels = _mpsk_with_carrier(m, 4000, f0, seed=13)
    c = CarrierMpsk(0.05, 0.707, 0.0, TSAMPS, 0.0, m)
    out = c.steps(rx)
    assert c.norm_freq == pytest.approx(f0, abs=3e-4)
    assert c.lock_metric > 0.9
    assert _symbol_errors(out, labels, m) == 0


def test_8psk_pull_in_with_fll():
    # 8PSK's narrow (+-pi/8) discriminator needs the FLL assist to acquire.
    rx, labels = _mpsk_with_carrier(8, 6000, 0.0015, seed=71)
    c = CarrierMpsk(0.05, 0.707, 0.0, TSAMPS, 0.01, 8)
    out = c.steps(rx)
    assert c.norm_freq == pytest.approx(0.0015, abs=5e-4)
    assert c.lock_metric > 0.9
    assert _symbol_errors(out, labels, 8) == 0


def test_noise_robust_qpsk():
    rx, labels = _mpsk_with_carrier(4, 5000, 0.0015, seed=2024, sigma=0.6)
    c = CarrierMpsk(0.03, 0.707, 0.0, TSAMPS, 0.0, 4)
    out = c.steps(rx)
    assert c.norm_freq == pytest.approx(0.0015, abs=5e-4)
    assert c.lock_metric > 0.7
    assert _symbol_errors(out, labels, 4) == 0


def test_fll_widens_pull_in():
    f0 = 0.006
    rx, labels = _mpsk_with_carrier(4, 6000, f0, seed=31)
    pll = CarrierMpsk(0.01, 0.707, 0.0, TSAMPS, 0.0, 4)
    pll.steps(rx)
    pll_locked = abs(pll.norm_freq - f0) < 5e-4 and pll.lock_metric > 0.9
    assert not pll_locked  # bare narrow PLL fails to acquire the big residual
    fll = CarrierMpsk(0.01, 0.707, 0.0, TSAMPS, 0.03, 4)
    out = fll.steps(rx)
    assert fll.norm_freq == pytest.approx(f0, abs=5e-4)
    assert fll.lock_metric > 0.9
    assert _symbol_errors(out, labels, 4) == 0


def test_reset_reproducible():
    rx, _ = _mpsk_with_carrier(8, 1500, 0.001, seed=55)
    c = CarrierMpsk(0.05, 0.707, 0.0, TSAMPS, 0.01, 8)
    o1 = c.steps(rx)
    f1, lk1 = c.norm_freq, c.lock_metric
    c.reset()
    o2 = c.steps(rx)
    assert np.array_equal(o1, o2)
    assert c.norm_freq == f1 and c.lock_metric == lk1


def test_empty_input():
    c = CarrierMpsk(m=4, tsamps=TSAMPS)
    out = c.steps(np.zeros(0, dtype=np.complex64))
    assert out.shape == (0,)
