#!/usr/bin/env python3
"""MpskReceiver demo: constellation pull-in, loop convergence, and BER.

Generates ``docs/assets/mpsk_receiver_demo.png`` (committed gallery asset):

  - Left   — QPSK received constellation, acquisition transient vs locked tail
             (a phase-rotating cloud collapses onto the 4 clusters).
  - Middle — the tracked carrier frequency and lock metric converging.
  - Right  — symbol error rate vs matched-filter Es/N0 for BPSK / QPSK / 8PSK,
             measured (NDA acquire + decision-directed handover) against the
             coherent M-PSK bound, ~1-2 dB implementation loss.

Run:  uv run python src/doppler/examples/mpsk_receiver_demo.py
"""

from __future__ import annotations

import sys

import numpy as np

from doppler.track import MpskReceiver

PHI0 = {2: 0.0, 4: np.pi / 4, 8: 0.0}


def _signal(m, sps, foff, esn0_db, nsym, seed):
    """Rectangular (I&D-matched) M-PSK at a carrier offset + AWGN.

    sigma is set so the *matched-filter-output* Es/N0 equals ``esn0_db``: a
    unit symbol through the length-sps boxcar has output noise sigma^2 / sps.
    """
    rng = np.random.default_rng(seed)
    idx = rng.integers(0, m, nsym)
    syms = np.exp(1j * (2 * np.pi * idx / m + PHI0[m])).astype(np.complex64)
    tx = np.repeat(syms, sps).astype(np.complex64)
    n = np.arange(tx.size)
    tx = tx * np.exp(1j * 2 * np.pi * foff * n)
    sigma = np.sqrt(sps / (2 * 10 ** (esn0_db / 10)))
    tx = tx + (
        rng.normal(0, sigma, tx.size) + 1j * rng.normal(0, sigma, tx.size)
    )
    return tx.astype(np.complex64), idx


def _ser(out, idx, m):
    th = np.angle(out) - PHI0[m]
    oi = np.round(th * m / (2 * np.pi)).astype(int) % m
    lo, hi = out.size // 4, out.size - out.size // 8
    best = 1.0
    for lag in range(-30, 31):
        base = np.arange(lo, hi) + lag
        if base.min() < 0 or base.max() >= idx.size:
            continue
        a, b = oi[lo:hi], idx[base]
        for r in range(m):
            best = min(best, float(np.mean(((a - b - r) % m) != 0)))
    return best


def _qfunc(x):
    from math import erfc, sqrt

    return 0.5 * erfc(x / sqrt(2.0))


def _theory_ser(m, esn0):
    if m == 2:
        return _qfunc(np.sqrt(2 * esn0))
    if m == 4:
        return 2 * _qfunc(np.sqrt(esn0))
    return 2 * _qfunc(np.sqrt(2 * esn0) * np.sin(np.pi / 8))


def main(out_path: str = "mpsk_receiver_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, (ax_c, ax_l, ax_b) = plt.subplots(1, 3, figsize=(13, 4.2))

    # ── Left + middle: QPSK pull-in and loop convergence ──────────────────
    # Seed the loop at zero while the signal carries a real offset, so the
    # acquisition transient (the cloud) is visible before lock.
    sps = 8
    foff = 0.0015
    tx, idx = _signal(4, sps, foff=foff, esn0_db=20, nsym=4000, seed=1)
    rx = MpskReceiver(
        m=4, sps=sps, n=4, init_norm_freq=0.0, bn_carrier=0.008, bn_timing=0.01
    )
    # process in fine blocks to log the loop state over time
    freqs, locks = [], []
    sym_chunks = []
    step = 256
    for i in range(0, tx.size, step):
        sym_chunks.append(rx.steps(tx[i : i + step]))
        freqs.append(rx.norm_freq)
        locks.append(rx.lock)
    out = np.concatenate(sym_chunks)
    # ── self-validation: the front-panel receiver really pulls in ────────
    # The tracked carrier must land on the injected offset, the lock
    # metric must rise well off its cold-start value, and the locked tail
    # must decode the transmitted symbols essentially error-free (the
    # coherent QPSK SER at Es/N0 = 20 dB is ~1e-23).
    ser_tail = _ser(out, idx, 4)
    print(
        f"QPSK pull-in: freq err {abs(freqs[-1] - foff):.2e} cyc/sample, "
        f"lock {locks[0]:.2f} -> {locks[-1]:.2f}, tail SER {ser_tail:.2e}"
    )
    assert abs(freqs[-1] - foff) < 1e-4, "carrier did not converge on f0"
    assert locks[-1] > 0.5, "lock metric never rose"
    assert ser_tail < 5e-4, "locked receiver failed to decode the symbols"
    early = out[:120]
    tail = out[-400:]
    ax_c.scatter(
        early.real, early.imag, s=6, c="tab:red", alpha=0.5, label="acquiring"
    )
    ax_c.scatter(
        tail.real, tail.imag, s=6, c="tab:blue", alpha=0.6, label="locked"
    )
    ax_c.set_aspect("equal")
    ax_c.set_title("QPSK pull-in: cloud → 4 clusters", fontsize=10)
    ax_c.set_xlabel("I")
    ax_c.set_ylabel("Q")
    ax_c.legend(fontsize=8, loc="upper right")
    ax_c.grid(alpha=0.3)

    t = np.arange(len(freqs)) * step / sps
    ax_l.plot(t, freqs, "-", color="tab:green", lw=1.4, label="tracked freq")
    ax_l.axhline(foff, color="k", ls="--", lw=1.0, label="true f0")
    ax_l.set_xlabel("symbol index")
    ax_l.set_ylabel("tracked freq (cycles/sample)", color="tab:green")
    ax_l.tick_params(axis="y", labelcolor="tab:green")
    axr = ax_l.twinx()
    axr.plot(t, locks, "-", color="tab:purple", lw=1.4, label="lock")
    axr.set_ylabel("lock metric", color="tab:purple")
    axr.tick_params(axis="y", labelcolor="tab:purple")
    ax_l.set_title("Carrier acquisition + lock", fontsize=10)
    ax_l.grid(alpha=0.3)

    # ── Right: BER vs Es/N0 per M (NDA acquire + DD handover) ──────────────
    orders = [
        (2, "BPSK", "tab:blue"),
        (4, "QPSK", "tab:orange"),
        (8, "8PSK", "tab:green"),
    ]
    db_grid = np.arange(4, 17, 2.0)
    for m, name, col in orders:
        meas = []
        sers = {}
        for db in db_grid:
            tx2, idx2 = _signal(m, sps, 0.0005, db, nsym=20000, seed=100 + m)
            rxm = MpskReceiver(
                m=m,
                sps=sps,
                n=4,
                init_norm_freq=0.0005,
                bn_carrier=0.005,
                bn_timing=0.005,
                acq_to_track=1,
                lock_thresh=0.3,
                warmup_syms=300,
            )
            out2 = rxm.steps(tx2)
            ser = _ser(out2, idx2, m)
            sers[db] = ser
            bps = {2: 1, 4: 2, 8: 3}[m]
            meas.append(max(ser / bps, 1e-6))  # ~BER via Gray
        # ── self-validation per order ─────────────────────────────────────
        # At the top of the grid the receiver decodes essentially
        # error-free; just above its acquisition threshold the measured
        # SER sits on the coherent bound to within the ~1-2 dB
        # implementation loss — a factor of a few in probability, never
        # orders of magnitude.
        assert sers[db_grid[-1]] < 2e-3, f"{name} did not decode at 16 dB"
        chk = {2: 6.0, 4: 8.0, 8: 14.0}[m]
        ratio = sers[chk] / _theory_ser(m, 10 ** (chk / 10))
        print(f"{name}: SER/theory at Es/N0 {chk:.0f} dB = {ratio:.2f}")
        assert 0.3 < ratio < 3.0, f"{name} SER departs from the bound"
        th = [
            max(_theory_ser(m, 10 ** (d / 10)) / {2: 1, 4: 2, 8: 3}[m], 1e-12)
            for d in db_grid
        ]
        ax_b.semilogy(db_grid, meas, "o", color=col, label=f"{name} meas")
        ax_b.semilogy(
            db_grid, th, "-", color=col, alpha=0.6, label=f"{name} bound"
        )
    ax_b.set_xlabel("matched-filter Es/N0 (dB)")
    ax_b.set_ylabel("BER")
    ax_b.set_ylim(1e-5, 1)
    ax_b.set_title("BER vs Es/N0 (acquire + handover)", fontsize=10)
    ax_b.legend(fontsize=7, ncol=3, loc="lower left")
    ax_b.grid(alpha=0.3, which="both")

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}  (QPSK tail SER {_ser(out, idx, 4):.2e})")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "mpsk_receiver_demo.png")
