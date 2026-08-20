import numpy as np

from doppler.viterbi import Viterbi


def test_create():
    obj = Viterbi(poly=np.zeros(1, dtype=np.uint32), k=7, invert=0, depth=35)
    assert obj is not None


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with Viterbi(poly=np.zeros(1, dtype=np.uint32), k=7, invert=0, depth=35):
        pass


def test_destroy():
    obj = Viterbi(poly=np.zeros(1, dtype=np.uint32), k=7, invert=0, depth=35)
    obj.destroy()
