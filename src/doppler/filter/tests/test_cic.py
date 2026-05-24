import pytest
import numpy as np
from doppler.filter import CIC


def test_create():
    obj = CIC(1, 4, 1)
    assert obj is not None

def test_getter_setter():
    pass  # no auto-state; add assertions for your fields

def test_reset():
    pass  # no auto-state; add assertions for your reset

def test_context_manager():
    with CIC(1, 4, 1) as obj:
        pass

def test_destroy():
    obj = CIC(1, 4, 1)
    obj.destroy()
