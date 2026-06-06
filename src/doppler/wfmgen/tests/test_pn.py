import pytest
import numpy as np
from doppler.wfmgen import PN


def test_create():
    obj = PN(96, 1, 7)
    assert obj is not None

def test_getter_setter():
    pass  # no auto-state; add assertions for your fields

def test_reset():
    pass  # no auto-state; add assertions for your reset

def test_context_manager():
    with PN(96, 1, 7) as obj:
        pass

def test_destroy():
    obj = PN(96, 1, 7)
    obj.destroy()
