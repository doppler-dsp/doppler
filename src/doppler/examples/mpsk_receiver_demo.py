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

# --8<-- [start:receiver]
import numpy as np

from doppler.ber import ber_settle_syms, ber_theory_ser
from doppler.track import MpskReceiver

# A QPSK signal at 8 samples/symbol with a residual carrier offset.
rng = np.random.default_rng(0)
idx = rng.integers(0, 4, 4000)
tx = np.exp(1j * (2 * np.pi * idx / 4 + np.pi / 4)).astype(np.complex64)
tx = np.repeat(tx, 8).astype(np.complex64)
k = np.arange(tx.size)
iq = (tx * np.exp(2j * np.pi * 0.0015 * k)).astype(np.complex64)

# Acquire blind (M-th-power NDA), then hand the shared LO over to
# low-jitter decision-directed tracking once locked and warmed up.
# bn_carrier is normalised to the SYMBOL rate, not the sample rate, and
# carrier PULL-IN range scales with it: acquiring a 0.0015 cyc/sample offset
# from a cold start (init_norm_freq defaults to 0) needs ~0.02 here.
rx = MpskReceiver(
    m=4,
    sps=8,
    m_out=4,
    pulse="iandd",
    bn_carrier=0.02,
    bn_timing=0.01,
    acq_to_track=1,
    # The lock statistic is normalised: ~1.0 at lock for every M, so this is a
    # plain fraction of what a locked constellation reads. It used to be scaled
    # per-M (QPSK peaked at 0.619), where 0.4 meant 0.4/0.619 = 65% of the
    # ceiling -- so 0.65 here is the SAME operating point, not a retune.
    lock_thresh=0.65,
    warmup_syms=200,
)
sym = rx.steps(iq)  # recovered symbols (~ len(iq) / sps)
bits = rx.bits(iq)  # hard Gray bits, LSB-first per symbol
assert rx.tracking == 1  # switched to decision-directed tracking
# --8<-- [end:receiver]

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


def _settle_floor(bn_timing, bn_carrier):
    """Symbols to allow for settling: `2*(5/bn_t + 5/bn_c)`.

    Delegates to `ber.ber_settle_syms` -- the C implementation is the only
    one. 5/Bn per loop, the two budgets ADD because the loops are cascaded,
    and the sum DOUBLES for joint tracking.
    """
    return ber_settle_syms(bn_timing, bn_carrier)


def _ser(out, idx, m, settle):
    """Steady-state symbol error rate, measured after ``settle`` symbols.

    Searches lag and constellation rotation because neither is observable from
    the output alone: the group delay depends on the pulse, the front end and
    the handover instant, and an NDA receiver locks to any of the M rotations.
    """
    th = np.angle(out) - PHI0[m]
    oi = np.round(th * m / (2 * np.pi)).astype(int) % m
    lo, hi = settle, out.size - out.size // 8
    assert hi - lo > 500, "settling budget leaves too few symbols to measure"
    best = 1.0
    # +-200, not +-30: a lag search clipped narrower than the delay it is
    # searching for reports chance SER on a perfectly healthy decode, which is
    # indistinguishable from a broken receiver. Cheap insurance -- the winning
    # lag here is 0, and the cost of being wrong about that is a false defect.
    for lag in range(-200, 201):
        base = np.arange(lo, hi) + lag
        if base.min() < 0 or base.max() >= idx.size:
            continue
        a, b = oi[lo:hi], idx[base]
        for r in range(m):
            best = min(best, float(np.mean(((a - b - r) % m) != 0)))
    return best


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
    bn_c, bn_t = 0.02, 0.01
    tx, idx = _signal(4, sps, foff=foff, esn0_db=20, nsym=4000, seed=1)
    rx = MpskReceiver(
        m=4,
        sps=sps,
        m_out=4,
        init_norm_freq=0.0,
        bn_carrier=bn_c,
        bn_timing=bn_t,
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
    # metric must rise well off its cold-start value, and the SETTLED tail
    # must decode the transmitted symbols essentially error-free (the
    # coherent QPSK SER at Es/N0 = 20 dB is ~1e-23). "Settled" is the
    # budget below, not a fraction of the record — measuring from 999
    # symbols reads SER 3.5e-2 on the same decode that is exactly 0 from
    # 1500, because the first 1500 are the joint acquisition transient.
    settle = _settle_floor(bn_t, bn_c)
    ser_tail = _ser(out, idx, 4, settle)
    print(
        f"QPSK pull-in: freq err {abs(freqs[-1] - foff):.2e} cyc/sample, "
        f"lock {locks[0]:.2f} -> {locks[-1]:.2f}, "
        f"tail SER {ser_tail:.2e} (from symbol {settle})"
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
                # m_out=8 is the default now, and this panel is the reason it
                # is: it measures against the coherent bound, and a
                # one-symbol-wide rectangle sampled only 4x/symbol leaves
                # 1-2 dB on the table (measured SER/theory 2.98/2.09/5.04 at
                # m_out=4 against 1.57/1.05/1.39 at 8, with EVM moving onto
                # -(Es/N0) exactly). Timing-error variance, not the carrier
                # loop -- the Gardner gate sits m_out/2 back. Passed
                # explicitly regardless, because this panel's claim is about
                # this value and should not move if the default ever does.
                m_out=8,
                init_norm_freq=0.0005,
                bn_carrier=0.005,
                bn_timing=0.005,
                acq_to_track=1,
                lock_thresh=0.3,
                warmup_syms=300,
            )
            out2 = rxm.steps(tx2)
            ser = _ser(out2, idx2, m, _settle_floor(0.005, 0.005))
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
        ratio = sers[chk] / ber_theory_ser(m, 10 ** (chk / 10))
        print(f"{name}: SER/theory at Es/N0 {chk:.0f} dB = {ratio:.2f}")
        assert 0.3 < ratio < 3.0, f"{name} SER departs from the bound"
        th = [
            max(
                ber_theory_ser(m, 10 ** (d / 10)) / {2: 1, 4: 2, 8: 3}[m],
                1e-12,
            )
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
    print(f"wrote {out_path}  (QPSK tail SER {ser_tail:.2e})")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "mpsk_receiver_demo.png")
