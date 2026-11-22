"""Integration tests for :class:`doppler.track.StateFilter`.

The algorithm lives in C and is pinned there
(``native/tests/state_filter/test_state_filter_core.c``); these tests hold
the binding to it, with oracles written in numpy that share no code with
the C: the recurrence, the loop's Parseval bandwidth, and the use case the
object exists for (a frequency ramp the second-order loop cannot null).
"""

import math

import numpy as np
import pytest

from doppler.track import LoopFilter, StateFilter

# The bound on bn*t the object accepts (header: STATE_FILTER_BNT_MAX).
BNT_MAX = 0.12


def ref_steps(x, kp, ki, kr, integ=0.0, rate=0.0):
    """The recurrence of the header, in plain Python floats."""
    out = np.empty(len(x))
    for n, e in enumerate(x):
        rate += kr * e
        integ += rate + ki * e
        out[n] = integ + kp * e
    return out, integ, rate


def parseval(sf, delay=0, gain=1.0, n=40000):
    """Bn = (1/2) sum phi^2 of the loop's impulse response, by running the
    real filter through the binding (integer delays only)."""
    r = np.zeros(delay + 1)  # r[0] = r[n], ... the detector's error history
    phi = 0.0
    total = 0.0
    for i in range(n):
        r[1:] = r[:-1].copy()
        r[0] = (1.0 if i == 0 else 0.0) - phi
        phi += sf.step(gain * r[delay])
        total += phi * phi
    return 0.5 * total


def test_defaults_and_properties():
    sf = StateFilter()
    assert (sf.bn, sf.t) == (0.01, 1.0)
    assert sf.kp > 0 and sf.ki > 0 and sf.kr > 0
    assert (sf.integ, sf.rate) == (0.0, 0.0)
    # the gains scale together: bn and t enter only as the product
    other = StateFilter(bn=0.005, t=2.0)
    assert (other.kp, other.ki, other.kr) == (sf.kp, sf.ki, sf.kr)


@pytest.mark.parametrize(
    "bn, t, names",
    [
        (-0.01, 1.0, "bn must be >= 0"),
        (0.01, 0.0, "t must be a positive"),
        (0.01, -1.0, "t must be a positive"),
        (float("nan"), 1.0, "bn must not be NaN"),
        (float("inf"), 1.0, "bn must be finite"),
        (0.01, float("inf"), "t must be a positive"),
        (1e-31, 1.0, "bn\\*t is below"),
        (BNT_MAX * 1.01, 1.0, "bn\\*t is above"),
        (0.01, BNT_MAX * 110, "bn\\*t is above"),
    ],
)
def test_create_refuses_outside_the_domain(bn, t, names):
    """Each refusal names the argument that failed, not the whole domain."""
    with pytest.raises(ValueError, match=names):
        StateFilter(bn=bn, t=t)


def test_configure_refuses_with_a_sentence_and_changes_nothing():
    sf = StateFilter(bn=0.02)
    before = (sf.kp, sf.ki, sf.kr, sf.bn, sf.t)
    with pytest.raises(ValueError, match="configure refused"):
        sf.configure(-1.0, 1.0)
    assert (sf.kp, sf.ki, sf.kr, sf.bn, sf.t) == before
    sf.configure(0.03, 1.0)  # the minimal pair: a good pair is accepted
    assert sf.bn == 0.03


def test_the_bound_itself_is_inside_and_bn_zero_is_frozen():
    StateFilter(bn=BNT_MAX, t=1.0)
    frozen = StateFilter(bn=0.0)
    assert (frozen.kp, frozen.ki, frozen.kr) == (0.0, 0.0, 0.0)
    assert frozen.step(5.0) == 0.0


def test_steps_follow_the_recurrence():
    rng = np.random.default_rng(3)
    x = rng.normal(size=300)
    sf = StateFilter(bn=0.04)
    want, integ, rate = ref_steps(x, sf.kp, sf.ki, sf.kr)
    got = sf.steps(x)
    np.testing.assert_allclose(got, want, rtol=1e-12, atol=1e-15)
    assert sf.integ == pytest.approx(integ, rel=1e-12)
    assert sf.rate == pytest.approx(rate, rel=1e-12)
    # one at a time is the same stream
    one = StateFilter(bn=0.04)
    np.testing.assert_allclose([one.step(e) for e in x], want, rtol=1e-12)


def test_seeded_estimates_move_the_output():
    # A caller who knows the frequency and its rate seeds both; the first
    # control value is then the seed carried one update, plus the error's
    # three gains.
    sf = StateFilter(bn=0.02)
    sf.integ = 0.5
    sf.rate = -0.01
    e = 0.2
    want = 0.5 + (-0.01 + sf.kr * e) + sf.ki * e + sf.kp * e
    assert sf.step(e) == pytest.approx(want, rel=1e-12)


def test_configure_keeps_the_estimates_and_a_refusal_changes_nothing():
    sf = StateFilter(bn=0.02)
    sf.steps(np.ones(25))
    integ, rate, kp = sf.integ, sf.rate, sf.kp
    sf.configure(0.05, 1.0)
    assert (sf.integ, sf.rate) == (integ, rate)
    assert sf.bn == 0.05 and sf.kp > kp
    before = (sf.kp, sf.ki, sf.kr, sf.integ, sf.rate, sf.bn, sf.t)
    for bn, t in ((-1.0, 1.0), (0.05, 0.0), (1.0, 1.0)):
        with pytest.raises(ValueError):
            sf.configure(bn, t)
    assert (sf.kp, sf.ki, sf.kr, sf.integ, sf.rate, sf.bn, sf.t) == before


def test_reset_zeroes_both_estimates_and_keeps_the_gains():
    sf = StateFilter(bn=0.03)
    sf.steps(np.ones(30))
    assert sf.integ != 0.0 and sf.rate != 0.0
    gains = (sf.kp, sf.ki, sf.kr)
    sf.reset()
    assert (sf.integ, sf.rate) == (0.0, 0.0)
    assert (sf.kp, sf.ki, sf.kr) == gains


@pytest.mark.parametrize("delay", [0, 1, 2])
def test_bandwidth_is_the_loops_parseval_sum(delay):
    # The exact function against a simulation of the real filter through
    # this binding. At delay 0 the delivered bandwidth is bn*t itself.
    sf = StateFilter(bn=0.03)
    exact = sf.bandwidth(delay=float(delay))
    assert exact == pytest.approx(
        parseval(StateFilter(bn=0.03), delay), rel=1e-6
    )
    if delay == 0:
        assert exact == pytest.approx(0.03, rel=1e-6)
    assert sf.stable(delay=float(delay)) is True


def test_gain_scales_the_loop_and_an_unstable_one_has_no_bandwidth():
    sf = StateFilter(bn=0.03)
    # a detector slope of 0.7 narrows the loop
    assert sf.bandwidth(gain=0.7) < sf.bandwidth()
    # past the upper edge for this delay the loop is unstable
    assert sf.stable(delay=2.0, gain=400.0) is False
    assert math.isnan(sf.bandwidth(delay=2.0, gain=400.0))
    # below the lower edge (about 1/(a*b) of nominal) it is unstable too
    assert sf.stable(gain=0.05) is False
    assert math.isnan(sf.bandwidth(gain=0.05))


def test_the_automatic_case_holds_over_the_rectangle():
    # bn*t at the bound, every gain from 0.5 to 2 and delay from 0 to 2.
    sf = StateFilter(bn=BNT_MAX)
    for delay in np.linspace(0.0, 2.0, 21):
        for gain in (0.5, 0.75, 1.0, 1.5, 2.0):
            assert sf.stable(delay=float(delay), gain=gain) is True, (
                delay,
                gain,
            )


def test_a_third_order_loop_nulls_a_ramp_the_second_order_cannot():
    # The use case: a frequency ramp of r per update into a closed loop.
    # Second order settles to a phase error r/ki; third order to zero.
    r = 1e-6
    sf = StateFilter(bn=0.05)
    lf = LoopFilter(bn=0.05, zeta=0.707, t=1.0)
    f = w = p3 = p2 = 0.0
    e3 = e2 = 0.0
    for _ in range(60000):
        f += r
        w += f
        e3 = w - p3
        p3 += sf.step(e3)
        e2 = w - p2
        p2 += lf.step(e2)
    assert abs(e3) < 1e-10
    assert e2 == pytest.approx(r / lf.ki, rel=1e-5)


def test_state_round_trip_and_mid_stream_split():
    rng = np.random.default_rng(5)
    x = rng.normal(size=200) * 0.1
    whole = StateFilter(bn=0.03)
    want = whole.steps(x)

    a = StateFilter(bn=0.03)
    a.steps(x[:90])
    blob = a.get_state()
    assert len(blob) == a.state_bytes()
    b = StateFilter(bn=0.03)
    b.set_state(blob)
    # both accumulators travel: the second half agrees bit for bit
    np.testing.assert_array_equal(b.steps(x[90:]), want[90:])
    assert (b.integ, b.rate) == (whole.integ, whole.rate)


def test_state_rejects():
    a = StateFilter(bn=0.03)
    a.steps(np.ones(10))
    blob = bytearray(a.get_state())
    b = StateFilter(bn=0.03)
    # envelope magic clobbered
    bad = bytearray(blob)
    bad[0] ^= 0xFF
    with pytest.raises(ValueError):
        b.set_state(bytes(bad))
    # the flavor word (first u32 after the 16-byte envelope) is a reject key
    bad = bytearray(blob)
    hdr = len(blob) - (8 + 7 * 8)
    bad[hdr] = 1
    with pytest.raises(ValueError):
        b.set_state(bytes(bad))
    # a gain that is not the one bn and t give
    bad = bytearray(blob)
    off = hdr + 8 + 2 * 8  # kr
    bad[off + 6] ^= 0x40  # a high mantissa bit: a relative change near 1e-3
    with pytest.raises(ValueError):
        b.set_state(bytes(bad))
    # the pad word (the u32 after the flavor) must be zero
    bad = bytearray(blob)
    bad[hdr + 4] = 1
    with pytest.raises(ValueError):
        b.set_state(bytes(bad))
    # a wrong size, and a non-bytes argument
    with pytest.raises(ValueError):
        b.set_state(bytes(blob[:-1]))
    with pytest.raises(TypeError):
        b.set_state("not bytes")
    # every refusal left the filter as it was
    assert (b.integ, b.rate) == (0.0, 0.0)


def test_context_manager_and_destroy():
    with StateFilter() as sf:
        assert isinstance(sf.step(1.0), float)
    sf = StateFilter()
    sf.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        sf.step(1.0)
