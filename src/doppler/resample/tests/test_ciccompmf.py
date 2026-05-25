"""Tests for filter.ciccompmf — CIC compensator FIR design."""

import pytest
import numpy as np
from doppler.resample import ciccompmf


# Reference taps from Python port of Hentschel/Fettweis algorithm (N=4, R=16)
_REF_M7 = np.array([
    -0.00465374,  0.05692651, -0.35183806,
     1.59913056,
    -0.35183806,  0.05692651, -0.00465374,
], dtype=np.float64)


def test_return_type():
    h = ciccompmf(4, 16, 7)
    assert isinstance(h, np.ndarray)
    assert h.dtype == np.float64


def test_length_odd():
    for m in [1, 3, 5, 7, 11, 19]:
        h = ciccompmf(4, 16, m)
        assert len(h) == m, f"M={m}"


def test_length_even():
    for m in [2, 4, 6, 8, 18]:
        h = ciccompmf(4, 16, m)
        assert len(h) == m, f"M={m}"


def test_dc_gain_odd():
    for m in [1, 3, 5, 7, 11, 19]:
        h = ciccompmf(4, 16, m)
        assert abs(sum(h) - 1.0) < 1e-10, f"M={m} DC gain {sum(h):.12f}"


def test_dc_gain_even():
    for m in [2, 4, 6, 8, 18]:
        h = ciccompmf(4, 16, m)
        assert abs(sum(h) - 1.0) < 1e-10, f"M={m} DC gain {sum(h):.12f}"


def test_symmetry_odd():
    h = ciccompmf(4, 16, 7)
    np.testing.assert_array_equal(h, h[::-1])


def test_symmetry_even():
    h = ciccompmf(4, 16, 6)
    np.testing.assert_array_equal(h, h[::-1])


def test_m1_identity():
    h = ciccompmf(4, 16, 1)
    assert len(h) == 1
    assert abs(h[0] - 1.0) < 1e-15


def test_reference_values_m7():
    h = ciccompmf(4, 16, 7)
    np.testing.assert_allclose(h, _REF_M7, atol=1e-7)


def test_passband_correction():
    """Compensator should boost near-passband frequencies to flatten CIC droop."""
    N, R, M = 4, 16, 7
    h = ciccompmf(N, R, M)
    n_fft = len(h) * 256
    H = np.abs(np.fft.rfft(h, n=n_fft))
    freqs = np.fft.rfftfreq(n_fft)
    # At passband edge (0.5/R in output-rate normalised units), the
    # compensator must have gain > 1 (it boosts to cancel CIC droop).
    pb_idx = int(round(0.5 / R * n_fft))
    assert H[pb_idx] > 1.0, "compensator must have gain > 1 at passband edge"


def test_invalid_m_too_large_odd():
    with pytest.raises(ValueError):
        ciccompmf(4, 16, 21)


def test_invalid_m_too_large_even():
    with pytest.raises(ValueError):
        ciccompmf(4, 16, 20)


def test_invalid_m_zero():
    with pytest.raises(ValueError):
        ciccompmf(4, 16, 0)


def test_different_r():
    """DC gain must be 1 regardless of R."""
    for r in [2, 4, 8, 16, 32, 64]:
        h = ciccompmf(4, r, 7)
        assert abs(sum(h) - 1.0) < 1e-10, f"R={r}"


def test_different_n():
    """DC gain must be 1 regardless of CIC order N."""
    for n in [1, 2, 4, 6, 8]:
        h = ciccompmf(n, 16, 7)
        assert abs(sum(h) - 1.0) < 1e-10, f"N={n}"
