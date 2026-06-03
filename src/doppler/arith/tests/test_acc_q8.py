import pytest
import numpy as np
from doppler.arith import AccQ8


def test_create():
    obj = AccQ8(0)
    assert obj is not None

def test_step_runs():
    obj = AccQ8(0)
    y = obj.step(1)
    assert y is None

def test_steps_runs():
    obj = AccQ8(0)
    x = np.ones(64, dtype=np.int8)
    assert obj.steps(x) is None

def test_getter_setter():
    obj = AccQ8(0)
    assert obj.get_acc() == 0
    obj.set_acc(2)
    assert obj.get_acc() == 2

def test_reset():
    obj = AccQ8(0)
    obj.set_acc(2)
    obj.reset()
    assert obj.get_acc() == 0

def test_context_manager():
    with AccQ8(0) as obj:
        y = obj.step(1)
    assert y is None

def test_destroy():
    obj = AccQ8(0)
    obj.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        obj.step(1)
