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


def test_steps_out_writes_into_callers_buffer():
    rx, _ = _bpsk_with_carrier(50, 0.0)
    c = Costas(0.03, 0.707, 0.0, TSAMPS)
    out = np.zeros(max(c.steps_max_out(), len(rx)), dtype=np.complex64)
    y = c.steps(rx, out=out)
    assert np.shares_memory(y, out)


def test_steps_out_undersized_raises():
    rx, _ = _bpsk_with_carrier(50, 0.0)
    c = Costas(0.03, 0.707, 0.0, TSAMPS)
    with pytest.raises(ValueError):
        c.steps(rx, out=np.zeros(1, dtype=np.complex64))


@pytest.mark.parametrize("bad", [float("nan"), -0.01, float("inf")])
def test_bad_bn_fll_is_refused_at_create_and_live(bad: float) -> None:
    """#2646: a negative/NaN bn_fll silently disabled the assist; inf blew
    the integrator up. Both faces now refuse it, and the getter is kept."""
    from doppler.track import CarrierMpsk, Costas

    for cls in (Costas, CarrierMpsk):
        with pytest.raises(ValueError, match="bn_fll >= 0"):
            cls(bn_fll=bad)
        c = cls(bn_fll=0.01)
        with pytest.raises(ValueError, match="bn_fll must be >= 0"):
            c.bn_fll = bad
        assert c.bn_fll == 0.01


def test_set_order_switches_and_carries_the_estimate():
    """order reads what set_order selected; the frequency estimate carries
    across a switch both ways, and a seeded rate is dropped on the way back
    to order 2."""
    c = Costas(bn=0.05, zeta=0.707, init_norm_freq=0.01, tsamps=TSAMPS)
    assert c.order == 2 and c.norm_rate == 0.0
    c.set_order(3, 0.02)
    assert c.order == 3 and c.bn == 0.02
    assert c.norm_freq == pytest.approx(0.01, abs=1e-15)
    c.norm_rate = 1e-9
    assert c.norm_rate == pytest.approx(1e-9, rel=1e-12)
    c.set_order(2, 0.05)
    assert c.order == 2 and c.norm_rate == 0.0
    assert c.norm_freq == pytest.approx(0.01, abs=1e-15)


def test_order_three_holds_a_ramp_order_two_lags():
    """The reason the order exists: on a frequency ramp order 2 holds a
    steady phase error, order 3 none, and order 3's rate is the ramp's."""
    nsym, ramp = 6000, 2e-8  # ramp: cycles/sample^2
    n = np.arange(nsym * TSAMPS)
    rng = np.random.default_rng(1)
    bits = np.where(rng.random(nsym) < 0.5, -1.0, 1.0)
    rx = (
        np.repeat(bits, TSAMPS) * np.exp(1j * 2 * np.pi * 0.5 * ramp * n**2)
    ).astype(np.complex64)
    lags = {}
    for order in (2, 3):
        c = Costas(bn=0.05, zeta=0.707, init_norm_freq=0.0, tsamps=TSAMPS)
        if order == 3:
            c.set_order(3, 0.05)
        e = []
        for s in range(nsym):
            c.steps(rx[s * TSAMPS : (s + 1) * TSAMPS])
            e.append(c.last_error)
        lags[order] = float(np.mean(e[-2000:]))
        if order == 3:
            assert c.norm_rate == pytest.approx(ramp, rel=1e-3)
    assert lags[2] > 1e-3
    assert abs(lags[3]) < 0.02 * lags[2]


@pytest.mark.parametrize(
    ("order", "bn"),
    [(4, 0.05), (1, 0.05), (3, 0.2), (3, -0.01)],
)
def test_set_order_refusals_change_nothing(order: int, bn: float) -> None:
    """Another order, or a bn outside the loop's domain (at most 0.12 at
    order 3), raises ValueError and leaves the loop as it was."""
    c = Costas(bn=0.05, zeta=0.707, init_norm_freq=0.01, tsamps=TSAMPS)
    blob = c.get_state()
    with pytest.raises(ValueError, match="set_order"):
        c.set_order(order, bn)
    assert c.get_state() == blob


def test_order_three_refuses_the_fll_assist_both_ways():
    """The FLL assist and order 3 do not combine, whichever comes first."""
    c = Costas(bn=0.05, zeta=0.707, tsamps=TSAMPS, bn_fll=0.01)
    with pytest.raises(ValueError, match="set_order"):
        c.set_order(3, 0.05)
    assert c.order == 2
    c.bn_fll = 0.0
    c.set_order(3, 0.05)
    with pytest.raises(ValueError, match="bn_fll"):
        c.bn_fll = 0.01
    assert c.bn_fll == 0.0


def test_norm_rate_is_refused_at_order_two():
    """Order 2 holds no rate: norm_rate reads 0 and refuses a write."""
    c = Costas(bn=0.05, zeta=0.707, tsamps=TSAMPS)
    with pytest.raises(ValueError, match="norm_rate"):
        c.norm_rate = 1e-9
    assert c.norm_rate == 0.0


def test_reset_keeps_order_three_and_zeroes_the_rate():
    """reset keeps the order, rewinds the frequency to the seed and zeroes
    the rate."""
    c = Costas(bn=0.05, zeta=0.707, init_norm_freq=0.003, tsamps=TSAMPS)
    c.set_order(3, 0.05)
    c.norm_rate = 1e-9
    c.reset()
    assert c.order == 3 and c.norm_rate == 0.0
    assert c.norm_freq == pytest.approx(0.003, abs=1e-15)
