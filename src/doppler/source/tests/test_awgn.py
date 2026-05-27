import pytest
import numpy as np
from doppler.source import AWGN


def test_create():
    obj = AWGN(0, 1.0)
    assert obj is not None

def test_getter_setter():
    pass  # no auto-state; add assertions for your fields

def test_reset():
    pass  # no auto-state; add assertions for your reset

def test_context_manager():
    with AWGN(0, 1.0) as obj:
        pass

def test_destroy():
    obj = AWGN(0, 1.0)
    obj.destroy()
