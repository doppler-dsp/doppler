import numpy as np
import pytest

from doppler.filter import MovingAverage


def test_create():
    obj = MovingAverage(4, 1.0)
    assert obj is not None


def test_step_runs():
    obj = MovingAverage(4, 1.0)
    y = obj.step(1.0 + 0.0j)
    assert isinstance(y, complex)


def test_steps_shape_dtype():
    obj = MovingAverage(4, 1.0)
    x = np.ones(64, dtype=np.complex64)
    y = obj.steps(x)
    assert y.shape == (64,)
    assert y.dtype == np.complex64


def test_steps_out_param():
    x = np.ones(64, dtype=np.complex64)
    buf = np.zeros(64, dtype=np.complex64)
    obj1 = MovingAverage(4, 1.0)
    ret = obj1.steps(x, buf)
    assert ret is buf
    obj2 = MovingAverage(4, 1.0)
    np.testing.assert_array_equal(ret, obj2.steps(x))


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with MovingAverage(4, 1.0) as obj:
        y = obj.step(1.0 + 0.0j)
    assert isinstance(y, complex)


def test_destroy():
    obj = MovingAverage(4, 1.0)
    obj.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        obj.step(1.0 + 0.0j)
