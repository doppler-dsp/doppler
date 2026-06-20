import numpy as np
import pytest

from doppler.arith import AccQ15


def test_create():
    obj = AccQ15(0)
    assert obj is not None


def test_step_runs():
    obj = AccQ15(0)
    y = obj.step(1)
    assert y is None


def test_steps_runs():
    obj = AccQ15(0)
    x = np.ones(64, dtype=np.int16)
    assert obj.steps(x) is None


def test_getter_setter():
    obj = AccQ15(0)
    assert obj.get_acc() == 0
    obj.set_acc(2)
    assert obj.get_acc() == 2


def test_reset():
    obj = AccQ15(0)
    obj.set_acc(2)
    obj.reset()
    assert obj.get_acc() == 0


def test_context_manager():
    with AccQ15(0) as obj:
        y = obj.step(1)
    assert y is None


def test_destroy():
    obj = AccQ15(0)
    obj.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        obj.step(1)
