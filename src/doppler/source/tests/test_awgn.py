import pytest
import numpy as np
from doppler.source import AWGN, awgn


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


def test_awgn_function_shape_dtype():
    y = awgn(1024)
    assert y.shape == (1024,)
    assert y.dtype == np.complex64


def test_awgn_function_amplitude():
    y = awgn(65536, amplitude=0.5)
    assert abs(float(np.std(np.real(y))) - 0.5) < 0.02


def test_awgn_function_seed():
    a = awgn(256, seed=42)
    b = awgn(256, seed=42)
    assert np.array_equal(a, b)


def test_awgn_function_different_seeds():
    a = awgn(256, seed=1)
    b = awgn(256, seed=2)
    assert not np.array_equal(a, b)
