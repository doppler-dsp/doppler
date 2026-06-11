import numpy as np
import pytest

from doppler.accumulator import AccTrace


def test_create_props():
    a = AccTrace(n=8, mode="mean")
    assert a.n == 8
    assert a.count == 0
    assert a.value() is None  # None before any accumulate


def test_mean():
    a = AccTrace(n=4, mode="mean")
    a.accumulate(np.array([1, 3, 5, 7], dtype=np.float32))
    a.accumulate(np.array([3, 5, 7, 9], dtype=np.float32))
    v = a.value()
    assert v.dtype == np.float32
    np.testing.assert_allclose(v, [2, 4, 6, 8], rtol=1e-6)
    assert a.count == 2


def test_maxhold_minhold():
    for mode, want in [("maxhold", [4, 5, 6]), ("minhold", [1, 3, 2])]:
        a = AccTrace(n=3, mode=mode)
        a.accumulate(np.array([1, 5, 2], dtype=np.float32))
        a.accumulate(np.array([4, 3, 6], dtype=np.float32))
        np.testing.assert_allclose(a.value(), want, rtol=1e-6)


def test_exp():
    a = AccTrace(n=2, mode="exp", alpha=0.5)
    a.accumulate(np.array([10, 20], dtype=np.float32))  # seed
    a.accumulate(np.array([2, 4], dtype=np.float32))  # 0.5*p + 0.5*acc
    np.testing.assert_allclose(a.value(), [6, 12], rtol=1e-6)


def test_reset():
    a = AccTrace(n=4, mode="mean")
    a.accumulate(np.ones(4, dtype=np.float32))
    a.reset()
    assert a.count == 0
    assert a.value() is None


def test_mode_property():
    assert AccTrace(n=4, mode="maxhold").mode == 2


def test_alpha_writable():
    a = AccTrace(n=4, mode="exp", alpha=0.1)
    a.alpha = 0.25
    assert abs(a.alpha - 0.25) < 1e-9


def test_bad_mode_raises():
    with pytest.raises(ValueError):
        AccTrace(n=4, mode="bogus")


def test_context_manager():
    with AccTrace(n=4, mode="mean") as a:
        a.accumulate(np.ones(4, dtype=np.float32))
        assert a.value() is not None
