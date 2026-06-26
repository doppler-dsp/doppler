"""Integration tests for doppler.resample.Farrow (fractional interpolator)."""

import numpy as np
import pytest

from doppler.resample import Farrow

ORDERS = ["linear", "parabolic", "cubic"]
# linear & the symmetric piecewise-parabolic are exact for degree 1; cubic
# (Lagrange) is exact for degree 3.
DEG = {"linear": 1, "parabolic": 1, "cubic": 3}


def test_create_and_group_delay():
    for o in ORDERS:
        f = Farrow(order=o)
        assert f.group_delay == 2


def test_context_manager_and_destroy():
    with Farrow(order="cubic"):
        pass
    Farrow(order="linear").destroy()


@pytest.mark.parametrize("order", ORDERS)
def test_polynomial_exactness(order):
    # each order reproduces a degree-`order` polynomial exactly
    t = np.arange(40.0)
    coeffs = np.array([0.4, -0.7, 0.3, -0.1])[: DEG[order] + 1]
    poly = sum(c * (t - 12.0) ** p for p, c in enumerate(coeffs))
    x = poly.astype(np.complex64)
    mu = 0.37
    f = Farrow(order=order)
    y = f.delay(x, mu)
    # y[i] is the input at i - group_delay + mu
    gd = f.group_delay
    idx = np.arange(gd + 2, 35)
    pos = idx - gd + mu
    want = sum(c * (pos - 12.0) ** p for p, c in enumerate(coeffs))
    assert np.allclose(y[idx].real, want, atol=1e-2)


def test_fractional_delay_of_sinusoid():
    n = 256
    fnorm = 0.05
    x = np.exp(2j * np.pi * fnorm * np.arange(n)).astype(np.complex64)
    f = Farrow(order="cubic")
    mu = 0.5
    y = f.delay(x, mu)
    idx = np.arange(8, n - 1)
    pos = idx - f.group_delay + mu
    want = np.exp(2j * np.pi * fnorm * pos)
    assert np.max(np.abs(y[idx] - want)) < 0.02


def test_lower_order_is_less_accurate_on_a_tone():
    # at a higher frequency, cubic beats parabolic beats linear
    n = 256
    fnorm = 0.18
    x = np.exp(2j * np.pi * fnorm * np.arange(n)).astype(np.complex64)
    idx = np.arange(8, n - 1)

    def err(order):
        f = Farrow(order=order)
        y = f.delay(x, 0.5)
        pos = idx - f.group_delay + 0.5
        want = np.exp(2j * np.pi * fnorm * pos)
        return np.sqrt(np.mean(np.abs(y[idx] - want) ** 2))

    assert err("cubic") < err("parabolic") < err("linear")


def test_reset():
    f = Farrow(order="linear")
    f.delay(np.ones(4, np.complex64), 0.5)
    f.reset()
    y = f.delay(np.zeros(4, np.complex64), 0.5)
    assert np.all(y == 0)
