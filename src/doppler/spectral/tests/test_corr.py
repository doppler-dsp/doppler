import numpy as np

from doppler.spectral import Corr


def test_create():
    obj = Corr(np.zeros(1, dtype=np.complex64), 1, 1)
    assert obj is not None


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with Corr(np.zeros(1, dtype=np.complex64), 1, 1):
        pass


def test_destroy():
    obj = Corr(np.zeros(1, dtype=np.complex64), 1, 1)
    obj.destroy()
