#!/usr/bin/env python3
"""A step, a ramp up, a ramp down — and what each one costs the carrier loop.

Generates ``docs/assets/mpsk_doppler_profile_demo.png`` (committed gallery
asset):

  - Top    — the Doppler profile the receiver is handed, and the frequency it
             tracks. A step at t=0, then a ramp up, then a ramp down, then
             flat again.
  - Middle — what the reported frequency ESTIMATE does. `rx.car.freq` is the
             loop filter's INTEGRATOR alone, so on a ramp it sits a constant
             distance below the truth — that distance is the proportional
             term, `kp * theta_ss`, which the applied NCO command includes and
             the readback does not.
  - Bottom — PHASE error, the loop stress. Zero on the flats, and a CONSTANT
             offset on each ramp whose size is a closed form and whose sign
             follows the ramp direction.

That contrast is the whole point. A frequency STEP is free in steady state, so
a test that only ever applies one cannot tell a correctly-sized loop from a
narrower one — which is exactly how `freq_scale` under-drove every non-strobe
tap for as long as it did (gh-765). A RAMP is what charges the loop, and what
it charges is:

    theta_ss = 2*pi*r / wn^2,   wn = 8*zeta*bn / (4*zeta^2 + 1)

with `r` the Doppler rate in cycles/symbol^2. The loop breaks when that lag
leaves the M-th-power discriminator's linear range, ~pi/(2M).

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

# `mf_in` is the default tap: it reads the matched filter's INPUT, so it needs
# no symbol timing. Nothing here depends on that choice — the loop stress
# below is a property of bn_carrier, not of where the detector reads.
rx = MpskReceiver(m=M, sps=SPS, m_out=4, bn_carrier=BN, bn_timing=0.005)

tlm = Telemetry()
rx.set_telemetry(tlm, "rx", 1)  # decim=1: every probe, every symbol
with MemoryCapture(tlm, BLOCK, SampleClock(FS)) as cap:
    for i in range(0, iq.size, BLOCK):
        tlm.set_now(i)
        rx.steps(iq[i : i + BLOCK])
series = cap.read_dict(index=True)  # {name: (sample_index, values)}

n_e, err = series["rx.car.e"]  # PHASE error — the loop stress
n_f, fhat = series["rx.car.freq"]  # the tracked frequency estimate
n_l, lock = series["rx.lock"]

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

    # 3. The reported frequency ESTIMATE lags each ramp by a constant, and
    #    that constant is PROPORTIONAL TO THE PHASE ERROR — because
    #    `rx.car.freq` is the integrator alone (mpsk_rx_freq_est), while the
    #    frequency the LO actually applies is `integ + kp*e`. So the deficit
    #    IS the proportional term. Checked as a ratio rather than a value:
    #    the same constant on both ramps, of opposite sign to theta, is what
    #    distinguishes "proportional term" from "the loop is lagging".
    ftrue = freq[np.clip(n_f, 0, freq.size - 1)]
    ferr = fhat - ftrue
    excursion = a * nseg
    lag_up = _tail(n_f, ferr, bounds[1], bounds[2]).mean()
    lag_dn = _tail(n_f, ferr, bounds[2], bounds[3]).mean()
    k_up, k_dn = lag_up / up, lag_dn / down
    print(
        f"estimate lag: up {lag_up:+.3e} down {lag_dn:+.3e} cyc/sample; "
        f"lag/theta {k_up:+.4e} vs {k_dn:+.4e}"
    )
    assert lag_up * lag_dn < 0, "the estimate lag did not follow the ramp"
    assert abs(k_up - k_dn) < 0.05 * abs(k_up), (
        "lag/theta is not the same constant on both ramps, so the deficit is "
        "not the proportional term"
    )
    assert abs(lag_up) < 0.05 * excursion, (
        "the estimate lag is not small against the frequency excursion"
    )

    fig, ax = plt.subplots(3, 1, figsize=(9.5, 8.0), sharex=True)
    t_all = np.arange(freq.size) / FS
    te, tf = n_e / FS, n_f / FS

    ax[0].plot(t_all, freq * FS, "k-", lw=1.2, label="Doppler profile")
    ax[0].plot(
        tf,
        fhat * FS,
        "-",
        color="tab:blue",
        lw=1.0,
        alpha=0.85,
        label="tracked (rx.car.freq)",
    )
    ax[0].set_ylabel("carrier (Hz)")
    rate_hz_s = RATE_SYM * (FS / SPS) ** 2
    ax[0].set_title(
        f"QPSK, Rs = {FS / SPS / 1e3:.0f} kSps, "
        f"bn_carrier = {BN}/symbol — Doppler rate "
        f"{rate_hz_s / 1e3:.1f} kHz/s on the ramps",
        fontsize=10,
    )
    ax[0].legend(fontsize=8, loc="lower right")

    ax[1].plot(tf, ferr * FS, "-", color="tab:green", lw=0.9)
    ax[1].axhline(0.0, color="k", lw=0.6, alpha=0.4)
    ax[1].set_ylabel("estimate − truth (Hz)")
    ax[1].set_title(
        "`rx.car.freq` is the INTEGRATOR alone, so on a ramp it sits a "
        "constant below truth: that gap is the\nproportional term kp*theta, "
        "which the applied NCO command carries and this readback does not.",
        fontsize=9,
    )

    # The raw per-symbol discriminator is data-noise dominated (+-0.4 rad);
    # the CLAIM is about its mean, so show both and let the mean carry it.
    w = 301
    sm = np.convolve(err, np.ones(w) / w, mode="same")
    ax[2].plot(
        te,
        err,
        "-",
        color="tab:red",
        lw=0.5,
        alpha=0.18,
        label="rx.car.e (per symbol)",
    )
    ax[2].plot(
        te[w:-w],
        sm[w:-w],
        "-",
        color="tab:red",
        lw=1.6,
        label=f"mean over {w} symbols",
    )
    for s, lab in ((+1, "law  +2pi r / wn^2"), (-1, None)):
        ax[2].axhline(s * theta_ss, color="k", ls="--", lw=0.9, label=lab)
    ax[2].axhline(
        linear_range,
        color="tab:orange",
        ls=":",
        lw=1.0,
        label=f"linear range +-pi/2M = {linear_range:.2f}",
    )
    ax[2].axhline(-linear_range, color="tab:orange", ls=":", lw=1.0)
    ax[2].set_ylabel("phase error (rad)")
    ax[2].set_xlabel("time (s)")
    ax[2].set_title(
        "...but it costs a CONSTANT PHASE error — the loop stress — whose "
        "size is the closed form and whose sign is the ramp's.",
        fontsize=9,
    )
    ax[2].set_ylim(-1.35 * linear_range, 1.35 * linear_range)
    ax[2].legend(fontsize=8, loc="upper left")

    for a_ in ax:
        a_.grid(alpha=0.3)
        for b in bounds[1:-1]:
            a_.axvline(b / FS, color="0.6", lw=0.8, ls="-", alpha=0.7)
    lo0, hi0 = ax[0].get_ylim()
    ax[0].set_ylim(lo0, hi0 + 0.18 * (hi0 - lo0))  # headroom for the labels
    for i, nm in enumerate(names):
        ax[0].text(
            (bounds[i] + bounds[i + 1]) / 2 / FS,
            ax[0].get_ylim()[1],
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
