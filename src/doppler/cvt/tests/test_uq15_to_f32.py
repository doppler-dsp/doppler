import numpy as np
import pytest

from doppler.cvt import UQ15ToF32


def test_create():
    obj = UQ15ToF32(32768.0)
    assert obj is not None


def test_step_runs():
    obj = UQ15ToF32(32768.0)
    y = obj.step(1)
    assert isinstance(y, float)


def test_steps_shape_dtype():
    obj = UQ15ToF32(32768.0)
    x = np.ones(64, dtype=np.uint16)
    y = obj.steps(x)
    assert y.shape == (64,)
    assert y.dtype == np.float32


def test_steps_out_param():
    x = np.ones(64, dtype=np.uint16)
    buf = np.zeros(64, dtype=np.float32)
    obj1 = UQ15ToF32(32768.0)
    ret = obj1.steps(x, buf)
    assert ret is buf
    obj2 = UQ15ToF32(32768.0)
    np.testing.assert_array_equal(ret, obj2.steps(x))


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with UQ15ToF32(32768.0) as obj:
        y = obj.step(1)
    assert isinstance(y, float)


def test_destroy():
    obj = UQ15ToF32(32768.0)
    obj.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        obj.step(1)
