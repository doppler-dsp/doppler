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
