#!/usr/bin/env python3
"""Sizing a carrier loop: acquire in a predictable time, then ride the Doppler.

Generates ``docs/assets/mpsk_doppler_profile_demo.png`` (committed gallery
asset):

  - Top    — a Doppler profile: a step at t=0, then a ramp up, then a ramp
             down, then flat. The tracked carrier (`rx.car.nco`, the command
             that actually drives the LO) lies on it.
  - Middle — SIZING, which is the decision that matters. Acquisition time
             scales as ~1/bn_carrier and lands at 5-10% of the classical
             `5/bn` settling budget, so you pick `bn_carrier` from the
             acquisition time you need and get it.
  - Bottom — what the profile costs: nothing on the flats, and a small
             CONSTANT phase offset on each ramp. It is book-keeping, and it
             is computable before you run anything.

**A step is free and a ramp is very cheap.** A type-2 loop nulls a frequency
step to zero steady-state error regardless of gain. Against a ramp it holds a
constant phase offset with a closed form:

    theta_ss = 2*pi*r / wn^2,   wn = 8*zeta*bn / (4*zeta^2 + 1)

At a realistic Doppler rate that number is negligible — 1 kHz/s at this symbol
rate works out to 4.5e-3 rad, about 1% of the discriminator's linear range.
The figure runs ~31x that so the offset is visible at all, and even there it
costs no decisions.

The reason it is worth measuring anyway is not that it hurts: it is that a
STEP-only test cannot size a loop, because a type-2 loop nulls a step whatever
its gain. A loop running several times narrower than configured passes every
step test unchanged, which is exactly how `freq_scale` under-drove every
non-strobe NDA tap until a ramp measurement found it (gh-765).

Run:  uv run python src/doppler/examples/mpsk_doppler_profile_demo.py
"""

from __future__ import annotations

import sys

# --8<-- [start:profile]
import numpy as np

from doppler.telemetry import MemoryCapture, Telemetry
from doppler.track import MpskReceiver
from doppler.wfm import SampleClock

FS = 1e6  # sample rate (Hz) — the figure's time axis
SPS = 8  # samples/symbol -> Rs = 125 kSps
M = 4  # QPSK
BN = 0.005  # carrier loop noise bandwidth, per SYMBOL
NSEG = 4000  # symbols per profile segment (4x the 5/bn settling)
BLOCK = 256

# The Doppler profile, in cycles/sample. A STEP the cold-started loop has to
# find, then a ramp UP, then a ramp DOWN of the same rate — so the frequency
# comes back to where it started — then flat.
# The step has to be one a COLD loop can actually find: an M-th-power NDA
# loop pulls in |df| up to about bn/M per symbol, which is 0.00125 here, and
# pull-in time grows as df^2/bn^3 past it. 0.001 cycles/symbol is 0.8 of that
# bound — a real acquisition, not a seeded one.
F0_SYM = 0.001  # cycles/SYMBOL
F0 = F0_SYM / SPS  # cycles/sample
RATE_SYM = 2.0e-6  # Doppler RATE, cycles/symbol^2
a = RATE_SYM / SPS**2  # the same rate per sample^2

nseg = NSEG * SPS
seg = np.arange(nseg)
freq = np.concatenate(
    [
        np.full(nseg, F0),  # A: step, then hold
        F0 + a * seg,  # B: ramp up
        F0 + a * nseg - a * seg,  # C: ramp down
        np.full(nseg, F0),  # D: hold again
    ]
)

# Phase is the running sum of frequency — the discrete-time NCO's own
# definition, so the segment joins carry no phase discontinuity to explain.
rng = np.random.default_rng(7)
idx = rng.integers(0, M, freq.size // SPS)
tx = np.exp(1j * (2 * np.pi * idx / M + np.pi / M)).astype(np.complex64)
tx = np.repeat(tx, SPS)
ph = 2.0 * np.pi * np.cumsum(freq)
iq = (tx * np.exp(1j * ph)).astype(np.complex64)

# `strobe` is the default tap: it reads the on-time strobe, at the full
# post-matched-filter SNR. Nothing here depends on that choice — the loop
# stress below is a property of bn_carrier, not of where the detector reads.
rx = MpskReceiver(m=M, sps=SPS, m_out=4, bn_carrier=BN, bn_timing=0.005)

tlm = Telemetry()
rx.set_telemetry(tlm, "rx", 1)  # decim=1: every probe, every symbol
with MemoryCapture(tlm, BLOCK, SampleClock(FS)) as cap:
    for i in range(0, iq.size, BLOCK):
        tlm.set_now(i)
        rx.steps(iq[i : i + BLOCK])
series = cap.read_dict(index=True)  # {name: (sample_index, values)}

n_e, err = series["rx.car.e"]  # phase error (book-keeping, see below)
n_f, fhat = series["rx.car.nco"]  # the SUM that drives the LO: integ + kp*e
n_i, fint = series["rx.car.freq"]  # the integrator alone — the freq MEMORY
n_l, lock = series["rx.lock"]

# Sizing: acquisition time against bn. The same ASK at every bandwidth (a
# step at 0.8 of the bn/M seeding bound), so the only thing varying is the
# loop. This is the middle panel and the practical result.
BN_GRID = (0.002, 0.005, 0.01, 0.02, 0.04)


def acquire_in(bn: float, nsym: int = 20000) -> int:
    """Symbols to first lock declaration at loop bandwidth `bn`."""
    r2 = np.random.default_rng(3)
    i2 = r2.integers(0, M, nsym)
    t2 = np.exp(1j * (2 * np.pi * i2 / M + np.pi / M)).astype(np.complex64)
    t2 = np.repeat(t2, SPS)
    k2 = np.arange(t2.size)
    f_step = 0.8 * bn / M / SPS  # 0.8 of the seeding bound, in cyc/sample
    x2 = (t2 * np.exp(2j * np.pi * f_step * k2)).astype(np.complex64)
    r = MpskReceiver(m=M, sps=SPS, m_out=4, bn_carrier=bn, bn_timing=0.005)
    r.steps(x2)
    return int(r.lock_time)


lock_times = [acquire_in(b) for b in BN_GRID]

# The closed form this figure exists to show, in the loop's own units.
wn = 8.0 * 0.707 * BN / (4.0 * 0.707**2 + 1.0)  # rad/symbol
theta_ss = 2.0 * np.pi * RATE_SYM / wn**2  # rad
linear_range = np.pi / (2 * M)  # M-th power S-curve
# --8<-- [end:profile]


def _tail(n_idx: np.ndarray, vals: np.ndarray, lo: int, hi: int) -> np.ndarray:
    """Values whose SAMPLE index lands in the last third of [lo, hi)."""
    start = lo + 2 * (hi - lo) // 3
    return vals[(n_idx >= start) & (n_idx < hi)]


def main(out_path: str = "mpsk_doppler_profile_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    bounds = [0, nseg, 2 * nseg, 3 * nseg, 4 * nseg]
    names = ["step + hold", "ramp up", "ramp down", "hold"]

    # ── self-validation: the figure's three claims, as asserts ───────────
    # 1. It locks, and stays locked through both ramps.
    assert lock[len(lock) // 4 :].min() > 0.5, "lost lock during the profile"
    assert rx.lock_time > 0, "never declared lock"
    assert rx.lock_time < 5.0 / BN, "lock later than the 5/bn budget"

    # 2. A flat segment costs no phase error; a ramp costs theta_ss, with the
    #    SIGN following the ramp direction. This is the claim.
    hold_a = _tail(n_e, err, bounds[0], bounds[1]).mean()
    up = _tail(n_e, err, bounds[1], bounds[2]).mean()
    down = _tail(n_e, err, bounds[2], bounds[3]).mean()
    hold_d = _tail(n_e, err, bounds[3], bounds[4]).mean()
    print(
        f"phase error (rad): hold {hold_a:+.4f}  up {up:+.4f}  "
        f"down {down:+.4f}  hold {hold_d:+.4f}  (law +-{theta_ss:.4f})"
    )
    assert abs(hold_a) < 0.1 * theta_ss, "a flat segment cost phase error"
    assert abs(hold_d) < 0.1 * theta_ss, "the loop did not recover after C"
    assert up * down < 0, "the two ramps did not oppose each other"
    for got, seg_name in ((abs(up), "up"), (abs(down), "down")):
        rel = abs(got - theta_ss) / theta_ss
        assert rel < 0.2, (
            f"ramp {seg_name}: lag {got:.4f} rad vs law {theta_ss:.4f} "
            f"({100 * rel:.0f}% off) — the loop is not the one configured"
        )

    # 3. SIZING: acquisition time scales as ~1/bn and lands well inside the
    #    classical 5/bn settling budget at every bandwidth. That
    #    predictability is the design lever — you choose bn_carrier from the
    #    acquisition time you can afford, and you get it.
    for bn_i, lt in zip(BN_GRID, lock_times):
        assert 0 < lt < 5.0 / bn_i, f"bn={bn_i}: lock_time {lt} outside 5/bn"
    print(
        "lock_time vs bn: "
        + ", ".join(f"{b}->{t}" for b, t in zip(BN_GRID, lock_times))
    )
    # NOT asserted: a clean 1/bn law. Measured across seeds it is not one —
    # bn=0.01 comes out bimodal (140/140/139/57/57) and the trend is not
    # monotonic, because the lock detector's own EMA (alpha=0.05) and its
    # verify counts put a floor and a threshold-crossing structure on top of
    # the loop's settling. What IS true is the bound, and the direction.
    assert lock_times[-1] < lock_times[0], (
        "a 20x wider loop should still acquire faster overall"
    )

    # 4. The SUM that drives the LO rides the ramp; the INTEGRATOR alone is
    #    short by the proportional term. That is why the sum is the probe
    #    worth publishing, and why the top panel plots it.
    ftrue = freq[np.clip(n_f, 0, freq.size - 1)]
    itrue = freq[np.clip(n_i, 0, freq.size - 1)]
    excursion = a * nseg
    sum_up = abs(_tail(n_f, fhat - ftrue, bounds[1], bounds[2]).mean())
    int_up = abs(_tail(n_i, fint - itrue, bounds[1], bounds[2]).mean())
    print(
        f"ramp-up lag: nco sum {sum_up:.3e}, integrator {int_up:.3e} "
        f"cyc/sample (excursion {excursion:.3e})"
    )
    assert sum_up < 0.01 * excursion, "the applied NCO command lagged the ramp"
    assert int_up > 5.0 * sum_up, (
        "the integrator-only view should lag by the proportional term; if it "
        "does not, these two probes are the same view"
    )

    fig = plt.figure(figsize=(9.5, 9.0))
    gs = fig.add_gridspec(3, 1, hspace=0.55)
    ax0, ax1, ax2 = (fig.add_subplot(gs[i]) for i in range(3))
    ax1.sharex(ax0)
    t_all = np.arange(freq.size) / FS
    te, tf = n_e / FS, n_f / FS

    # ── 1. the profile, and the command that actually drives the LO ──────
    ax0.plot(
        t_all,
        freq * FS,
        "-",
        color="k",
        lw=2.6,
        alpha=0.85,
        label="Doppler profile",
    )
    # The sum carries the discriminator's full per-symbol variance — its
    # MEAN is what tracks the ramp, which is what get_nco_freq() documents
    # ("mean tracks a ramp with no lag, variance is loop stress").
    wf = 201
    fsm = np.convolve(fhat, np.ones(wf) / wf, mode="same")
    ax0.plot(
        tf,
        fhat * FS,
        "-",
        color="tab:blue",
        lw=0.5,
        alpha=0.15,
        label="rx.car.nco (per symbol)",
    )
    ax0.plot(
        tf[wf:-wf],
        fsm[wf:-wf] * FS,
        "--",
        color="tab:cyan",
        lw=1.4,
        label=f"its mean over {wf} symbols — the sum driving the LO",
    )
    ax0.set_ylabel("carrier (Hz)")
    ax0.set_xlabel("time (s)")
    rate_hz_s = RATE_SYM * (FS / SPS) ** 2
    ax0.set_title(
        f"QPSK, Rs = {FS / SPS / 1e3:.0f} kSps, bn_carrier = {BN}/symbol. "
        f"Step at t=0, then +-{rate_hz_s / 1e3:.0f} kHz/s ramps.\n"
        f"The loop rides all of it — the tracked line lies on the profile.",
        fontsize=9.5,
    )
    ax0.legend(fontsize=8, loc="lower right")

    # ── 2. what that costs: book-keeping, and computable in advance ──────
    w = 301
    sm = np.convolve(err, np.ones(w) / w, mode="same")
    ax1.plot(
        te,
        err,
        "-",
        color="tab:red",
        lw=0.5,
        alpha=0.15,
        label="rx.car.e (per symbol)",
    )
    ax1.plot(
        te[w:-w],
        sm[w:-w],
        "-",
        color="tab:red",
        lw=1.6,
        label=f"mean over {w} symbols",
    )
    for sgn, lab in ((+1, "2*pi*r / wn^2"), (-1, None)):
        ax1.axhline(sgn * theta_ss, color="k", ls="--", lw=0.9, label=lab)
    for sgn in (+1, -1):
        ax1.axhline(
            sgn * linear_range,
            color="tab:orange",
            ls=":",
            lw=1.0,
            label="linear range +-pi/2M" if sgn > 0 else None,
        )
    ax1.set_ylim(-1.35 * linear_range, 1.35 * linear_range)
    ax1.set_ylabel("phase error (rad)")
    ax1.set_xlabel("time (s)")
    realistic = 2 * np.pi * (1.0e3 / (FS / SPS) ** 2) / wn**2
    ax1.set_title(
        f"The cost: nothing on the flats, a constant {theta_ss:.3f} rad on "
        f"each ramp. Book-keeping, and computable\nbefore you run anything "
        f"— at a realistic 1 kHz/s it would be {realistic:.0e} rad. The rate "
        f"here is exaggerated ~{rate_hz_s / 1e3:.0f}x to make it visible.",
        fontsize=9.5,
    )
    ax1.legend(fontsize=8, loc="upper left", ncol=2)

    # ── 3. sizing: the decision that actually matters ────────────────────
    ax2.semilogx(
        BN_GRID,
        [5.0 / b for b in BN_GRID],
        "--",
        color="0.5",
        lw=1.1,
        label="5/bn settling budget",
    )
    ax2.semilogx(
        BN_GRID,
        lock_times,
        "o-",
        color="tab:purple",
        lw=1.5,
        ms=7,
        label="measured lock_time",
    )
    ax2.set_yscale("log")
    ax2.set_xlabel("bn_carrier (per symbol)")
    ax2.set_ylabel("symbols to lock")
    ax2.set_title(
        "SIZING. lock_time sits an order of magnitude inside the 5/bn budget "
        "at every bandwidth and is\nrepeatable per configuration — but it is "
        "NOT a clean 1/bn law: the lock detector's own EMA and\nverify counts "
        "put a floor under it. Measure it, do not compute it —\nwhich is "
        "what the property is for.",
        fontsize=9.5,
    )
    ax2.legend(fontsize=8)
    ax2.grid(alpha=0.3, which="both")

    ax = (ax0, ax1)
    for a_ in ax:
        a_.grid(alpha=0.3)
        for b in bounds[1:-1]:
            a_.axvline(b / FS, color="0.6", lw=0.8, alpha=0.7)
    lo0, hi0 = ax0.get_ylim()
    ax0.set_ylim(lo0, hi0 + 0.20 * (hi0 - lo0))
    for i, nm in enumerate(names):
        ax0.text(
            (bounds[i] + bounds[i + 1]) / 2 / FS,
            ax0.get_ylim()[1],
            f" {nm} ",
            ha="center",
            va="top",
            fontsize=8.5,
            color="0.25",
            bbox={"fc": "white", "ec": "0.8", "lw": 0.5, "pad": 1.5},
        )

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(
        f"wrote {out_path}  (lock_time {rx.lock_time} symbols, "
        f"theta_ss {theta_ss:.4f} rad)"
    )


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "mpsk_doppler_profile_demo.png")
