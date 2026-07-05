import numpy as np
import pytest

from doppler.measure import ToneMeasure


def test_create():
    obj = ToneMeasure(8192, 1.0, 8, 1.0, 0, 90.0, 0)
    assert obj is not None


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with ToneMeasure(8192, 1.0, 8, 1.0, 0, 90.0, 0):
        pass


def test_destroy():
    obj = ToneMeasure(8192, 1.0, 8, 1.0, 0, 90.0, 0)
    obj.destroy()


def test_spectrum_dbfs_out_writes_into_callers_buffer():
    n = 8192
    obj = ToneMeasure(n, 1.0, 8, 1.0, 0, 90.0, 0)
    x = np.zeros(n, dtype=np.float32)
    out = np.zeros(max(obj.spectrum_dbfs_max_out(), len(x)), dtype=np.float32)
    y = obj.spectrum_dbfs(x, out=out)
    assert np.shares_memory(y, out)


def test_spectrum_dbfs_out_undersized_raises():
    n = 8192
    obj = ToneMeasure(n, 1.0, 8, 1.0, 0, 90.0, 0)
    with pytest.raises(ValueError):
        obj.spectrum_dbfs(
            np.zeros(n, dtype=np.float32), out=np.zeros(1, dtype=np.float32)
        )
