"""Integration tests for :class:`doppler.track.KalmanStateFilter`.

The algorithm lives in C and is pinned there
(``native/tests/state_filter_kalman/test_state_filter_kalman_core.c``); these
tests hold the binding to it with oracles in numpy that share no code with
the C: the Riccati equation itself, and the recurrence the filter runs.
"""

import numpy as np
import pytest

from doppler.track import KalmanStateFilter, StateFilter

F = np.array([[1.0, 1, 0], [0, 1, 1], [0, 0, 1]])


def riccati(q, r):
    """The steady state of P' = F P F' - S K K' + Q by plain iteration in
    numpy (a separate implementation from the C doubling solve): returns the
    covariance and the loop gains (kp, ki, kr)."""
    P = np.eye(3) * r
    Q = np.diag([0.0, 0.0, q])
    for _ in range(400000):
        S = P[0, 0] + r
        K = F @ P[:, 0] / S
        P2 = F @ P @ F.T - S * np.outer(K, K) + Q
        done = np.max(np.abs(P2 - P)) <= 1e-15 * np.max(np.abs(P2))
        P = P2
        if done:
            break
    S = P[0, 0] + r
    K = F @ P[:, 0] / S
    return P, (K[0] - K[1] + K[2], K[1] - 2 * K[2], K[2])


def test_the_two_forms_agree():
    b = KalmanStateFilter(sigma_e=1.0, sigma_a=1e-3)
    a = KalmanStateFilter(bn=b.bandwidth())
    assert a.kp == pytest.approx(b.kp, rel=1e-6)
    assert a.ki == pytest.approx(b.ki, rel=1e-6)
    assert a.kr == pytest.approx(b.kr, rel=1e-6)


def test_steady_state_is_the_riccati_solution():
    f = KalmanStateFilter(sigma_e=1.0, sigma_a=1e-3, t=1.0)
    _, (kp, ki, kr) = riccati(1e-6, 1.0)
    assert f.kp == pytest.approx(kp, rel=1e-6)
    assert f.ki == pytest.approx(ki, rel=1e-6)
    assert f.kr == pytest.approx(kr, rel=1e-6)


def test_the_automatic_case_is_constant_gain():
    f = KalmanStateFilter(bn=0.02)
    gains = (f.kp, f.ki, f.kr)
    f.steps(np.full(200, 0.1))
    # constant to rounding: the covariance stays at its fixed point
    assert (f.kp, f.ki, f.kr) == pytest.approx(gains, rel=1e-12)
    assert f.integ != 0.0


def test_a_larger_start_raises_the_gains_then_they_return():
    P, (_kp, _ki, kr) = riccati(4e-12, 1.0)
    sf = float(np.sqrt(P[1, 1]))
    # the frequency uncertainty is in the loop's units, bounded by the
    # steady state's own deviation below and 5 times it above
    with pytest.raises(ValueError):
        KalmanStateFilter(sigma_e=1.0, sigma_a=2e-6, sigma_f0=0.5 * sf)
    with pytest.raises(ValueError):
        KalmanStateFilter(sigma_e=1.0, sigma_a=2e-6, sigma_f0=5.5 * sf)
    f = KalmanStateFilter(sigma_e=1.0, sigma_a=2e-6, sigma_f0=4.0 * sf)
    peak = 0.0
    for _ in range(20000):
        f.step(0.0)
        peak = max(peak, f.kr)
    assert peak > 1.2 * kr
    f.steps(np.zeros(100_000))
    assert f.kr == pytest.approx(kr, rel=1e-6)


@pytest.mark.parametrize(
    "kw, names",
    [
        ({}, "bn alone"),
        ({"sigma_e": 1.0, "sigma_a": 1e-3, "bn": 0.01}, "not both"),
        ({"bn": 0.5}, r"bn\*t is outside"),  # beyond the flavor's bound
        ({"bn": -0.01}, "bn must be finite and positive"),
        ({"bn": 0.01, "t": 0.0}, "t must be a positive"),
        ({"sigma_e": 1.0, "sigma_a": -1e-3}, "must both be > 0"),
        ({"sigma_e": 1.0, "sigma_a": 1e-24}, "below the smallest"),
        ({"sigma_e": 1.0, "sigma_a": 1.0}, "above the largest"),
        ({"sigma_e": 1.0, "sigma_a": 1e-3, "sigma_f0": 1e9}, "sigma_f0 must"),
    ],
)
def test_a_refusal_names_the_argument_at_create(kw, names):
    """Each refusal says which argument failed, not the whole domain."""
    with pytest.raises(ValueError, match=names):
        KalmanStateFilter(**kw)
    # the minimal pair: the form that is right builds
    KalmanStateFilter(bn=0.01)
    KalmanStateFilter(sigma_e=1.0, sigma_a=1e-3)


def test_configure_names_only_what_changes():
    f = KalmanStateFilter(sigma_e=1.0, sigma_a=1e-3)
    with pytest.raises(ValueError, match="configure refused"):
        f.configure(sigma_e=1.0, sigma_a=1e-3, bn=0.01)  # mixing the forms
    with pytest.raises(ValueError, match="configure refused"):
        f.configure(bn=0.5)
    g = (f.kp, f.ki, f.kr)
    f.configure(sigma_a=2e-3)  # only the rate-walk figure changes
    assert (f.kp, f.ki, f.kr) != g
    f.steps(np.full(30, 0.1))
    integ = f.integ
    f.configure()  # no arguments: restart the schedule, figures as they are
    assert f.integ == integ
    f.configure(sigma_a=1e-3)
    assert (f.kp, f.ki, f.kr) == pytest.approx(g, rel=1e-12)


def test_configure_with_no_arguments_ends_a_high_gain_start():
    steady = KalmanStateFilter(sigma_e=1.0, sigma_a=2e-6)
    f = KalmanStateFilter(
        sigma_e=1.0, sigma_a=2e-6, sigma_f0=3.0 * steady.sigma_f
    )
    f.steps(np.full(60, 0.05))
    assert f.kr != pytest.approx(steady.kr, rel=1e-3)  # mid-transient
    integ = f.integ
    f.configure()  # the gains go to the steady state, the estimates carry
    assert f.integ == integ
    assert (f.kp, f.ki, f.kr) == pytest.approx(
        (steady.kp, steady.ki, steady.kr), rel=1e-12
    )
    f.configure(sigma_f0=3.0 * steady.sigma_f)  # sigma_f0 alone re-opens them
    assert f.sigma_f == pytest.approx(steady.sigma_f, rel=1e-9)


def test_a_bn_call_replaces_the_stored_figures():
    f = KalmanStateFilter(sigma_e=2.0, sigma_a=1e-3)
    f.configure(bn=0.02)
    g = KalmanStateFilter(bn=0.02)
    assert (f.kp, f.ki, f.kr) == pytest.approx((g.kp, g.ki, g.kr), rel=1e-12)
    # a figures-form call that follows keeps the implied sigma_e of 1
    f.configure(sigma_a=2e-3)
    h = KalmanStateFilter(sigma_e=1.0, sigma_a=2e-3)
    assert (f.kp, f.ki, f.kr) == pytest.approx((h.kp, h.ki, h.kr), rel=1e-9)


def test_the_automatic_case_holds_the_implied_figures():
    f = KalmanStateFilter(bn=0.01)
    bn0 = f.bandwidth()
    f.configure(sigma_e=2.0)  # sigma_a keeps its implied value
    # a detector twice as noisy at the same rate-walk figure narrows the loop
    # by the sixth root of the noise ratio: 4^(-1/6) = 0.794
    assert 0.75 < f.bandwidth() / bn0 < 0.83


def test_configure_keeps_the_estimates():
    f = KalmanStateFilter(bn=0.01)
    f.steps(np.full(100, 0.05))
    integ, rate = f.integ, f.rate
    f.configure(bn=0.02)
    assert (f.integ, f.rate) == (integ, rate)
    assert abs(f.bn * f.t - 0.02) < 1e-9


def test_reset_returns_to_the_start():
    f = KalmanStateFilter(sigma_e=1.0, sigma_a=1e-3)
    g = (f.kp, f.ki, f.kr)
    f.steps(np.full(300, 0.1))
    f.reset()
    assert (f.integ, f.rate) == (0.0, 0.0)
    assert (f.kp, f.ki, f.kr) == g


def test_the_read_surface_is_the_bases():
    f = KalmanStateFilter(bn=0.01)
    base = StateFilter(bn=0.01)
    # same shape of answer; the gains differ by the Riccati-vs-Butterworth gap
    assert f.stable() and base.stable()
    assert f.bandwidth() == pytest.approx(0.01, rel=1e-6)
    assert abs(f.kr / base.kr - 1.0) < 0.02


def test_state_round_trip_resumes_bit_for_bit():
    x = np.random.default_rng(3).normal(0, 0.2, 400)
    a = KalmanStateFilter(sigma_e=1.0, sigma_a=2e-3)
    whole = a.steps(x)
    b = KalmanStateFilter(sigma_e=1.0, sigma_a=2e-3)
    b.steps(x[:150])
    blob = b.get_state()
    assert len(blob) == b.state_bytes()
    c = KalmanStateFilter(bn=0.05)
    c.set_state(blob)
    np.testing.assert_array_equal(c.steps(x[150:]), whole[150:])


def test_a_fixed_gain_blob_is_refused_and_the_other_way():
    k = KalmanStateFilter(bn=0.01)
    s = StateFilter(bn=0.01)
    with pytest.raises(ValueError):
        k.set_state(s.get_state())
    with pytest.raises(ValueError):
        s.set_state(k.get_state())
