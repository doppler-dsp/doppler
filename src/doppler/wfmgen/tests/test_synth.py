import pytest
import numpy as np
from doppler.wfmgen import Synth


def test_create():
    obj = Synth(0, 1000000.0, 0.0, 100.0, 1)
    assert obj is not None

def test_step_runs():
    obj = Synth(0, 1000000.0, 0.0, 100.0, 1)
    y = obj.step()
    assert isinstance(y, complex)

def test_steps_shape_dtype():
    obj = Synth(0, 1000000.0, 0.0, 100.0, 1)
    y = obj.steps(64)
    assert y.shape == (64,)
    assert y.dtype == np.complex64

def test_getter_setter():
    pass  # no auto-state; add assertions for your fields

def test_reset():
    pass  # no auto-state; add assertions for your reset

def test_context_manager():
    with Synth(0, 1000000.0, 0.0, 100.0, 1) as obj:
        y = obj.step()
    assert isinstance(y, complex)

def test_destroy():
    obj = Synth(0, 1000000.0, 0.0, 100.0, 1)
    obj.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        obj.step()
