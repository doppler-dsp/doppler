import pytest
import numpy as np
from doppler.cvt import I32ToF32


def test_create():
    obj = I32ToF32(2147483648.0)
    assert obj is not None

def test_step_runs():
    obj = I32ToF32(2147483648.0)
    y = obj.step(1)
    assert isinstance(y, float)

def test_steps_shape_dtype():
    obj = I32ToF32(2147483648.0)
    x = np.ones(64, dtype=np.int32)
    y = obj.steps(x)
    assert y.shape == (64,)
    assert y.dtype == np.float32

def test_steps_out_param():
    x   = np.ones(64, dtype=np.int32)
    buf = np.zeros(64, dtype=np.float32)
    obj1 = I32ToF32(2147483648.0)
    ret = obj1.steps(x, buf)
    assert ret is buf
    obj2 = I32ToF32(2147483648.0)
    np.testing.assert_array_equal(ret, obj2.steps(x))

def test_getter_setter():
    pass  # no auto-state; add assertions for your fields

def test_reset():
    pass  # no auto-state; add assertions for your reset

def test_context_manager():
    with I32ToF32(2147483648.0) as obj:
        y = obj.step(1)
    assert isinstance(y, float)

def test_destroy():
    obj = I32ToF32(2147483648.0)
    obj.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        obj.step(1)
