"""mpsk_constellation_demo.py — Gray-coded M-PSK map/demap, and its BER.

Shows :mod:`doppler.mpsk` — the M-PSK constellation primitive the MPSK receiver
composes. Two views (saved to a PNG):

  * **Constellations** — BPSK / QPSK / 8PSK on the unit circle, each point
    annotated with its Gray label. Adjacent points (in phase) differ by exactly
    one bit, so a symbol slip costs one bit error.
  * **BER vs Eb/N0** — map -> complex AWGN -> hard demap, Monte-Carlo, overlaid
    on the closed-form curves (BPSK == Gray QPSK; 8PSK Gray high-SNR approx).
    The simulated points sit on the theory, validating the constellation and
    the slicer end to end.

Run:  python -m doppler.examples.mpsk_constellation_demo  [out.png]
"""

from __future__ import annotations

import math
import sys

import numpy as np

from doppler.mpsk import mpsk_bits_per_symbol, mpsk_demap, mpsk_map

M_LIST = [
    (2, "BPSK", "#1f77b4"),
    (4, "QPSK", "#2ca02c"),
    (8, "8PSK", "#d62728"),
]


def _qfunc(x):
    return 0.5 * math.erfc(x / math.sqrt(2.0))


def _theory_ber(m, ebn0):
    bps = mpsk_bits_per_symbol(m)
    if m <= 4:
        return _qfunc(math.sqrt(2 * ebn0))  # BPSK == Gray QPSK
    esn0 = ebn0 * bps
    return 2 * _qfunc(math.sqrt(2 * esn0) * math.sin(math.pi / 8)) / bps


def _sim_ber(m, ebn0_db, rng, n=300_000):
    bps = mpsk_bits_per_symbol(m)
    tx = rng.integers(0, m, n).astype(np.uint8)
    s = mpsk_map(tx, m)
    esn0 = 10 ** (ebn0_db / 10.0) * bps
    sigma = math.sqrt(1.0 / esn0 / 2.0)
    noise = sigma * (rng.standard_normal(n) + 1j * rng.standard_normal(n))
    rx = mpsk_demap((s + noise).astype(np.complex64), m)
    xor = (tx ^ rx).astype(np.uint8)
    nerr = int(np.count_nonzero(xor[:, None] & (1 << np.arange(bps))))
    return nerr / (n * bps)


def main(out_path="mpsk_constellation_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    rng = np.random.default_rng(0)
    fig, (a, b) = plt.subplots(1, 2, figsize=(11.5, 4.6))

    # --- 1. constellations with Gray labels ---
    for m, name, col in M_LIST:
        pts = mpsk_map(np.arange(m, dtype=np.uint8), m)
        # self-validation: every point on the unit circle, and walking the
        # circle in phase order the Gray labels differ by exactly one bit
        # (a symbol slip to a neighbour costs exactly one bit error).
        assert np.max(np.abs(np.abs(pts) - 1.0)) < 1e-3, f"{name} off circle"
        ring = np.arange(m)[np.argsort(np.angle(pts))]
        flips = ring ^ np.roll(ring, 1)
        assert all(bin(int(v)).count("1") == 1 for v in flips), (
            f"{name} labels are not Gray-coded around the circle"
        )
        a.scatter(pts.real, pts.imag, s=40, color=col, label=name, zorder=3)
        if m == 8:  # annotate the densest one's Gray labels
            for g in range(m):
                p = pts[g]
                a.annotate(
                    format(g, "03b"),
                    (p.real, p.imag),
                    textcoords="offset points",
                    xytext=(6, 4),
                    fontsize=7,
                    color=col,
                )
    th = np.linspace(0, 2 * np.pi, 256)
    a.plot(np.cos(th), np.sin(th), color="#cccccc", lw=0.8, zorder=1)
    a.axhline(0, color="k", lw=0.5)
    a.axvline(0, color="k", lw=0.5)
    a.set_aspect("equal")
    a.set_xlim(-1.5, 1.7)
    a.set_ylim(-1.5, 1.5)
    a.set_title(
        "Gray-coded M-PSK constellations\n"
        "(8PSK labels shown; adjacent = 1 bit)",
        fontsize=9,
    )
    a.set_xlabel("I")
    a.set_ylabel("Q")
    a.legend(fontsize=8, loc="upper right")
    a.grid(alpha=0.2)

    # --- 2. BER vs Eb/N0: simulation vs theory ---
    ebn0_db = np.arange(0, 13)
    for m, name, col in M_LIST:
        theory = [_theory_ber(m, 10 ** (e / 10.0)) for e in ebn0_db]
        b.semilogy(ebn0_db, theory, color=col, lw=1.3, label=f"{name} theory")
        # simulate where BER is measurable with the sample budget
        sim_db = ebn0_db[(ebn0_db >= 2) & (ebn0_db <= (10 if m < 8 else 12))]
        sim = [_sim_ber(m, e, rng) for e in sim_db]
        b.semilogy(
            sim_db, sim, "o", color=col, ms=5, mfc="none", label=f"{name} sim"
        )
        # self-validation: map -> AWGN -> demap must land on the closed
        # form. Judge only points with >= 100 expected bit errors, where
        # the Monte-Carlo spread is a few percent, not order-one.
        th_s = np.array([_theory_ber(m, 10 ** (e / 10.0)) for e in sim_db])
        nbits = 300_000 * mpsk_bits_per_symbol(m)
        well = th_s * nbits >= 100
        ratio = np.asarray(sim)[well] / th_s[well]
        print(
            f"{name}: BER/theory over {int(well.sum())} points: "
            f"{ratio.min():.3f}..{ratio.max():.3f}"
        )
        assert np.all((ratio > 0.7) & (ratio < 1.4)), (
            f"{name} simulated BER departs from theory"
        )
    b.set_ylim(1e-5, 1)
    b.set_title("BER vs Eb/N0 — simulation on theory", fontsize=9)
    b.set_xlabel("Eb/N0 (dB)")
    b.set_ylabel("bit error rate")
    b.legend(fontsize=7, ncol=2, loc="lower left")
    b.grid(alpha=0.3, which="both")

    fig.suptitle(
        "doppler.mpsk — Gray-coded M-PSK map / demap (BPSK / QPSK / 8PSK)",
        fontsize=11,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "mpsk_constellation_demo.png")
