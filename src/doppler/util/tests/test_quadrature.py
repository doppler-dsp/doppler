"""The binding face of util's sinc and quadrature primitives.

The properties themselves are pinned in C (``test_util_core.c`` §9-§14);
these check that the Python face reaches the same definitions: the out
arrays are filled in place, the rules integrate what they claim, and a bad
length is refused rather than answered.
"""

import math

import numpy as np
import pytest

from doppler.util import (
    complement_power,
    ema_alpha_decim,
    gauss_hermite,
    mean_sinc,
    midpoint_nodes,
    simpson_weights,
    sinc,
)


def test_sinc_matches_numpy():
    u = np.linspace(-3.3, 3.3, 41)
    assert [sinc(float(v)) for v in u] == pytest.approx(np.sinc(u), abs=1e-15)


def test_complement_power_is_the_ema_kernel():
    # One expression, two library quantities: at integer d it is the EMA
    # coefficient, bit for bit.
    for d in (2, 3, 8, 64):
        assert complement_power(0.05, float(d)) == ema_alpha_decim(0.05, d)


def test_gauss_hermite_moments_of_the_normal():
    z, p = np.empty(6), np.empty(6)
    gauss_hermite(z, p)
    assert p.sum() == pytest.approx(1.0, abs=1e-13)
    # exact up to degree 2n - 1 = 11: E[Z^k] = (k-1)!! for even k
    for k in range(1, 12):
        want = 0.0 if k % 2 else math.prod(range(k - 1, 0, -2))
        assert float(p @ z**k) == pytest.approx(want, abs=1e-10)


def test_simpson_and_midpoint_average_a_function():
    w = np.empty(33)
    simpson_weights(w)
    u = np.linspace(0.0, 2.0, 33)
    assert float(w @ np.sinc(u)) == pytest.approx(mean_sinc(2.0), abs=1e-6)
    m = np.empty(5)
    midpoint_nodes(m)
    assert m.tolist() == pytest.approx([0.1, 0.3, 0.5, 0.7, 0.9], abs=1e-15)


@pytest.mark.parametrize(
    "call",
    [
        lambda: simpson_weights(np.empty(4)),  # even length
        lambda: gauss_hermite(np.empty(3), np.empty(4)),  # mismatched
    ],
)
def test_a_bad_length_is_refused(call):
    # RuntimeError until jm can name the exception for a module function
    # (just-buildit/just-makeit#1614).
    with pytest.raises(RuntimeError, match="rc=-4"):
        call()
