"""pn_codes.py — PN/LFSR codes: length, realization, and the 64-bit register.

Demonstrates the maximum-length-sequence properties and the two LFSR
realizations exposed by `doppler.wfm.PN`, plus the 64-bit register path
(lengths > 32). Runs in a second with no plotting — every claim is asserted.

Run:
    python examples/python/pn_codes.py
"""

from __future__ import annotations

import numpy as np

from doppler.wfm import PN

# Right-shift Galois primitive polynomials (the engine's built-in MLS table).
POLY = {7: 0x41, 9: 0x108, 15: 0x4001}


def maximal_length(poly: int, n: int, lfsr: str) -> np.ndarray:
    """Return one period of the m-sequence and check the MLS invariants:
    period 2**n - 1, balanced to 2**(n-1) ones."""
    period = (1 << n) - 1
    chips = np.asarray(PN(poly, 1, n, lfsr=lfsr).generate(2 * period))
    assert np.array_equal(chips[:period], chips[period : 2 * period]), "period"
    assert int(chips[:period].sum()) == (1 << (n - 1)), "balance"
    return chips[:period]


def main() -> None:
    # ── Galois vs Fibonacci: same polynomial, same period, different order ──
    for n in (7, 9, 15):
        g = maximal_length(POLY[n], n, "galois")
        f = maximal_length(POLY[n], n, "fibonacci")
        assert not np.array_equal(g, f)
        period = (1 << n) - 1
        print(
            f"n={n:2d}  period={period:6d}  ones={g.sum():6d}  "
            f"galois≠fibonacci ✓"
        )

    # ── Periodic autocorrelation: the MLS "thumbtack" (1 at 0, −1/period) ──
    n = 9
    period = (1 << n) - 1
    bits = maximal_length(POLY[n], n, "galois")
    bipolar = 1 - 2 * bits.astype(np.float64)  # 0/1 → +1/−1
    ac = np.array(
        [np.dot(bipolar, np.roll(bipolar, k)) / period for k in range(period)]
    )
    assert np.isclose(ac[0], 1.0)
    assert np.allclose(ac[1:], -1.0 / period, atol=1e-9)
    print(f"n={n}  autocorrelation: peak={ac[0]:.3f}, floor={ac[1]:.5f}")

    # ── 64-bit register (length 40): auto-MLS poly, balanced, never collapses
    big = np.asarray(PN(0x800000001C, 1, 40).generate(100_000))
    assert set(np.unique(big).tolist()) <= {0, 1}
    assert 0.45 < big.mean() < 0.55 and big.any()
    print(f"n=40 (64-bit): {len(big)} chips, mean={big.mean():.3f} ✓")

    print("\nall PN code checks passed.")


if __name__ == "__main__":
    main()
