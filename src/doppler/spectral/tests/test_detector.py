import pytest
import numpy as np
from doppler.spectral import Detector


def test_create():
    obj = Detector(np.zeros(1, dtype=np.complex64), "mean", 1, 0, n-1, 0.0, 1)
    assert obj is not None

def test_getter_setter():
    pass  # no auto-state; add assertions for your fields

def test_reset():
    pass  # no auto-state; add assertions for your reset

def test_context_manager():
    with Detector(np.zeros(1, dtype=np.complex64), "mean", 1, 0, n-1, 0.0, 1) as obj:
        pass

def test_destroy():
    obj = Detector(np.zeros(1, dtype=np.complex64), "mean", 1, 0, n-1, 0.0, 1)
    obj.destroy()
