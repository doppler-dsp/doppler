import numpy as np
import pytest

from doppler.track import LoopFilter


def test_create():
    obj = LoopFilter(0.01, 0.707, 1.0)
    assert obj is not None


def test_step_runs():
    obj = LoopFilter(0.01, 0.707, 1.0)
    y = obj.step(1.0)
    assert isinstance(y, float)


def test_steps_shape_dtype():
    obj = LoopFilter(0.01, 0.707, 1.0)
    x = np.ones(64, dtype=np.float64)
    y = obj.steps(x)
    assert y.shape == (64,)
    assert y.dtype == np.float64


def test_steps_out_param():
    x = np.ones(64, dtype=np.float64)
    buf = np.zeros(64, dtype=np.float64)
    obj1 = LoopFilter(0.01, 0.707, 1.0)
    ret = obj1.steps(x, buf)
    assert ret is buf
    obj2 = LoopFilter(0.01, 0.707, 1.0)
    np.testing.assert_array_equal(ret, obj2.steps(x))


def test_gains_match_closed_form():
    bn, zeta, t = 0.02, 0.707, 1.0
    lf = LoopFilter(bn, zeta, t)
    wn = 8 * zeta * bn / (4 * zeta**2 + 1)
    th = wn * t
    den = 4 + 4 * zeta * th + th**2
    assert lf.kp == pytest.approx(8 * zeta * th / den)
    assert lf.ki == pytest.approx(4 * th * th / den)
    # first update on a unit error: integ = ki, control = ki + kp
    assert lf.step(1.0) == pytest.approx(lf.ki + lf.kp)
    assert lf.integ == pytest.approx(lf.ki)


def test_reset_zeros_integrator():
    lf = LoopFilter(0.02, 0.707, 1.0)
    for _ in range(5):
        lf.step(1.0)
    assert lf.integ > 0
    kp = lf.kp
    lf.reset()
    assert lf.integ == 0.0
    assert lf.kp == kp


def test_configure_preserves_integrator():
    lf = LoopFilter(0.02, 0.707, 1.0)
    lf.step(2.0)
    integ, kp = lf.integ, lf.kp
    lf.configure(0.05, 0.707, 1.0)
    assert lf.integ == integ
    assert lf.bn == 0.05
    assert lf.kp != kp


def test_context_manager():
    with LoopFilter(0.01, 0.707, 1.0) as obj:
        y = obj.step(1.0)
    assert isinstance(y, float)


def test_destroy():
    obj = LoopFilter(0.01, 0.707, 1.0)
    obj.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        obj.step(1.0)


@pytest.mark.parametrize(
    "bn, zeta, t",
    [
        (float("nan"), 0.707, 1.0),
        (-1.25, 1.0, 1.0),  # zeroed the gains' denominator
        (0.01, float("inf"), 1.0),
        (0.01, 0.707, 0.0),
    ],
)
def test_configure_refuses_outside_the_domain(bn, zeta, t):
    """configure() is as untrusted as the constructor: a NaN gave gains that
    never recover, and a negative bn with zeta >= 1 divided by zero
    (doppler#2103). It refuses outside dp_loop_filter_params_ok(), writing
    nothing: the whole state, gains and integrator, is unchanged."""
    lf = LoopFilter(bn=0.01, zeta=0.707, t=1.0)
    lf.step(1.0)
    held = lf.get_state()
    with pytest.raises(ValueError, match="configure failed"):
        lf.configure(bn, zeta, t)
    assert lf.get_state() == held


@pytest.mark.parametrize("field", [0, 1, 3, 4, 5])  # kp, ki, bn, zeta, t
def test_set_state_refuses_a_non_finite_payload(field):
    """A forged NaN gain or loop parameter is refused, target unchanged.

    The blob is the whole struct after a 16-byte envelope; the envelope
    never reads the payload, so only the payload check (#2641) can refuse
    the NaN gains that create() refuses (#2103). The integrator (field 2)
    is not checked: a NaN there is garbage in (#2128).
    """
    lf = LoopFilter(0.01, 0.707, 1.0)
    lf.steps(np.full(30, 0.1))
    before = lf.get_state()
    hdr = len(before) - 6 * 8
    payload = np.frombuffer(before, dtype=np.float64, offset=hdr).copy()
    payload[field] = np.nan
    forged = before[:hdr] + payload.tobytes()
    with pytest.raises(ValueError):
        lf.set_state(forged)
    assert lf.get_state() == before
