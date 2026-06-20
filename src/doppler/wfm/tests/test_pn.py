"""Integration tests for the PN (LFSR m-sequence) generator.

Covers maximum-length behaviour for primitive polynomials, the Galois vs
Fibonacci realizations, the 64-bit register path (lengths > 32), explicit
polynomials, reset reproducibility, and the lifecycle surface.
"""

import numpy as np
import pytest

from doppler.wfm import PN

# Right-shift Galois primitive polynomials (the wfm_synth_mls_poly table)
# — each yields a maximum-length sequence for its register length.
POLY = {5: 0x12, 7: 0x41, 9: 0x108}

# n=40 primitive polynomial — exercises the 64-bit LFSR path (poly > 2**32).
POLY_40 = 0x800000001C


def test_create():
    assert PN(96, 1, 7) is not None


def test_context_manager():
    with PN(96, 1, 7):
        pass


def test_destroy():
    PN(96, 1, 7).destroy()


def test_generate_is_binary():
    chips = np.asarray(PN(POLY[7], 1, 7).generate(500))
    assert chips.dtype == np.uint8
    assert set(np.unique(chips).tolist()) <= {0, 1}


@pytest.mark.parametrize("n", [5, 7, 9])
def test_galois_maximal_length(n):
    """A primitive polynomial yields an MLS: period 2**n - 1, with 2**(n-1)
    ones per period (the balance property of a maximum-length sequence)."""
    period = (1 << n) - 1
    chips = np.asarray(PN(POLY[n], 1, n).generate(2 * period))
    assert np.array_equal(chips[:period], chips[period : 2 * period])
    assert int(chips[:period].sum()) == (1 << (n - 1))


@pytest.mark.parametrize("n", [5, 7, 9])
def test_fibonacci_matches_galois_period(n):
    """The Fibonacci realization of the same polynomial has the same maximal
    period and balance, but a different chip sequence/phase than Galois."""
    period = (1 << n) - 1
    g = np.asarray(PN(POLY[n], 1, n, lfsr="galois").generate(2 * period))
    f = np.asarray(PN(POLY[n], 1, n, lfsr="fibonacci").generate(2 * period))
    assert np.array_equal(f[:period], f[period : 2 * period])  # maximal
    assert int(f[:period].sum()) == (1 << (n - 1))  # balanced
    assert not np.array_equal(g, f)  # distinct realization


def test_64bit_length():
    """Lengths above 32 use the full 64-bit register. The period (2**40 - 1)
    is too long to verify exhaustively, so check the sequence is binary,
    balanced, and never collapses to the all-zero fixed point."""
    chips = np.asarray(PN(POLY_40, 1, 40).generate(50_000))
    assert set(np.unique(chips).tolist()) <= {0, 1}
    assert 0.45 < chips.mean() < 0.55
    assert chips.any()


def test_64bit_fibonacci_runs():
    """Fibonacci taps are derived from the (64-bit) Galois polynomial."""
    chips = np.asarray(PN(POLY_40, 1, 40, lfsr="fibonacci").generate(50_000))
    assert 0.45 < chips.mean() < 0.55


def test_reset_reproduces():
    p = PN(POLY[7], 1, 7)
    a = np.asarray(p.generate(127)).copy()
    p.reset()
    assert np.array_equal(a, np.asarray(p.generate(127)))


def test_explicit_poly_changes_sequence():
    a = np.asarray(PN(POLY[7], 1, 7).generate(127))
    b = np.asarray(PN(POLY[9], 1, 9).generate(127))
    assert not np.array_equal(a[:64], b[:64])
