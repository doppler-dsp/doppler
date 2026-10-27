import numpy as np
import pytest

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


def test_dwell_zero_is_refused_as_value_error():
    """dwell=0 never dumps, so create refuses it as ValueError (#2641)."""
    ref = np.zeros(8, np.complex64)
    ref[0] = 1.0
    with pytest.raises(ValueError, match="dwell >= 1"):
        Corr(ref=ref, dwell=0)
