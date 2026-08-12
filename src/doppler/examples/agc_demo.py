"""agc_demo.py — AGC decimated-loop convergence demo with plots.

Feeds the AGC a constant-envelope tone whose power steps by 20 dB partway
through, and runs the same input through three decimation settings —
decim = 1, 8, 16 — at one fixed loop bandwidth.

agc_steps() runs the detector + loop filter once per chunk of `decim`
samples, but COMPOUNDS both per-chunk coefficients from `loop_bw` / `alpha`
(k_c = 1 - (1 - 4*loop_bw)**c, alpha_c = 1 - (1 - alpha)**c).  Compounding
rather than scaling linearly by `c` is what keeps them their per-sample
meaning, so all three decim settings share one effective loop bandwidth and
converge on top of each other — decim only coarsens the path, not the
destination.

How MUCH it coarsens the path is set by a single number, `4*decim*loop_bw`:
how far the loop moves within one chunk.  The rule is to keep it at or
below 0.05, and the third panel is that rule made visible — the same step
response run at two bandwidths that straddle it.

Saves a three-panel plot to agc_convergence.png:
  - top    : input vs output power (dB) for each decim, with the reference
  - middle : applied gain (dB) for each decim — the gain actually seen by
             each sample.  agc_steps() commands a new gain once per chunk
             but applies it as a first-order hold, ramping linearly across
             the chunk, so the trace is smooth rather than a staircase.
  - bottom : the step-response FAMILY.  decim 8/16/32 at 4*decim*loop_bw =
             0.032 (inside the rule, curves indistinguishable) and at 0.32
             (six times the rule, curves visibly fanned).  Time is in loop
             time constants so the two bandwidths overlay.

Run:
  python examples/python/agc_demo.py
"""

import matplotlib

matplotlib.use("Agg")  # headless: render straight to a file, no display

import matplotlib.pyplot as plt

# --8<-- [start:step_response]
import numpy as np

from doppler.agc import AGC

N_TOTAL = 6000  # total samples processed
N_STEP = 3000  # sample index where the input level jumps
F_TONE = 0.02  # normalised tone frequency (cycles/sample)
REF_DB = 0.0  # AGC target output power
LOOP_BW = 0.00125  # loop noise bandwidth (fixed for all decim)
ALPHA = 0.02  # power-detector EMA coefficient
LO_DB = -10.0  # input power before the step
HI_DB = 10.0  # input power after the step

# Constant-envelope tone whose power steps LO_DB -> HI_DB at sample N_STEP.
n = np.arange(N_TOTAL)
amp = np.where(n < N_STEP, 10.0 ** (LO_DB / 20.0), 10.0 ** (HI_DB / 20.0))
x = (amp * np.exp(2j * np.pi * F_TONE * n)).astype(np.complex64)

agc = AGC(ref_db=REF_DB, loop_bw=LOOP_BW, alpha=ALPHA)
agc.decim = 8  # update loop every 8 samples
y = agc.steps(x)  # normalised output, power → REF_DB
# --8<-- [end:step_response]

DECIMS = (1, 8, 16)  # decimation factors compared at one loop bandwidth


def run(decim):
    """Process the input through agc_steps() at the given decimation.

    Returns the output power and the applied gain, both per sample.  The
    applied gain is recovered directly from the data as |y| / |x| — for a
    constant-envelope input that is exactly the first-order-hold gain
    agc_steps() ramped onto each sample, with no staircase artefact.
    """
    agc = AGC(REF_DB, LOOP_BW, ALPHA)
    agc.decim = decim
    y = agc.steps(x)
    out_db = 10.0 * np.log10(np.abs(y) ** 2)
    applied_db = 20.0 * np.log10(np.abs(y) / np.abs(x))
    return out_db, applied_db, agc


in_db = 10.0 * np.log10(np.abs(x) ** 2)

runs = {}
print("=== AGC decimated-loop convergence (fixed loop bandwidth) ===")
print(f"input power: {LO_DB:.0f} dB -> {HI_DB:.0f} dB at sample {N_STEP}")
for d in DECIMS:
    out_db, applied_db, agc = run(d)
    runs[d] = (out_db, applied_db)
    # Settling time: samples after the step until the output stays within
    # 1 dB of the reference.
    post = np.abs(out_db[N_STEP:] - REF_DB) <= 1.0
    settle = int(np.argmax(post)) if post.any() else -1
    # applied_gain_db is the queryable telemetry: the gain the last sample
    # actually saw, as opposed to gain_db (what the loop now commands).
    print(
        f"decim {d:2d}: settles within 1 dB of {REF_DB:.0f} dB "
        f"{settle} samples after the step; "
        f"applied_gain_db={agc.applied_gain_db:+.2f} "
        f"(commanded gain_db={agc.gain_db:+.2f})"
    )
    # The loop must genuinely converge, and decim must only coarsen the
    # path — not move the destination.  Once transients die out (well
    # before the last 1000 samples of each segment) the output power has
    # to sit on the reference, and the steady-state gain has to cancel
    # the input level exactly (REF_DB - HI_DB after the step).
    assert 0 < settle <= 1000, (
        f"decim {d}: no settling within 1000 samples (settle={settle})"
    )
    pre = out_db[N_STEP - 1000 : N_STEP]
    post = out_db[N_TOTAL - 1000 :]
    assert np.max(np.abs(pre - REF_DB)) < 1.0, (
        f"decim {d}: pre-step output not converged to {REF_DB} dB"
    )
    assert np.max(np.abs(post - REF_DB)) < 1.0, (
        f"decim {d}: post-step output not converged to {REF_DB} dB"
    )
    assert abs(agc.applied_gain_db - (REF_DB - HI_DB)) < 0.5, (
        f"decim {d}: steady-state gain {agc.applied_gain_db:+.2f} dB, "
        f"expected {REF_DB - HI_DB:+.1f} dB"
    )

# ── The rule, as a family of step responses ──────────────────────────────
# `4*decim*loop_bw` is how far the loop moves within one chunk, and it is
# the only quantity that decides whether decim is free. Two bandwidths that
# straddle the 0.05 rule, each run at decim 8/16/32, from a cold start so
# the whole transient is visible rather than the tail of one.
FAMILY_DECIMS = (8, 16, 32)
GROUP_IN = 0.032  # inside the rule
GROUP_OUT = 0.32  # six times it — where the anomaly was found
# Each group is quoted at the LARGEST decim, so it bounds the whole family.
BW_IN = GROUP_IN / (4.0 * max(FAMILY_DECIMS))
BW_OUT = GROUP_OUT / (4.0 * max(FAMILY_DECIMS))


def step_family(loop_bw):
    """Applied gain vs time-in-loop-time-constants, one trace per decim.

    A cold start into a constant -20 dB input: the loop must climb 20 dB,
    which is the transient the rule is about. Time is normalised by the
    loop time constant 1/(4*loop_bw) so families at different bandwidths
    lie on the same axis and can be compared by eye.
    """
    tau = 1.0 / (4.0 * loop_bw)
    n_fam = int(6 * tau)
    amp_fam = 10.0 ** (-20.0 / 20.0)
    xf = (amp_fam * np.exp(2j * np.pi * F_TONE * np.arange(n_fam))).astype(
        np.complex64
    )
    traces = {}
    for d in FAMILY_DECIMS:
        a = AGC(REF_DB, loop_bw, ALPHA)
        a.decim = d
        yf = a.steps(xf)
        traces[d] = 20.0 * np.log10(np.abs(yf) / np.abs(xf))
    return np.arange(n_fam) / tau, traces


def worst_spread(traces):
    """Largest gap between any two decims, compared fairly.

    Sampled at indices where every decim has just FINISHED a chunk (one
    less than a multiple of the largest decim). Comparing at arbitrary
    indices would measure the first-order hold's ramp phase instead: at
    sample 20 a decim-32 loop is mid-ramp while a decim-8 loop committed
    its gain 4 samples ago, and that intra-chunk sawtooth is inherent to
    the hold rather than a difference in trajectory. Aligning to chunk
    ends is what makes this the same quantity the rule is stated for.
    """
    step = max(FAMILY_DECIMS)
    idx = np.arange(step - 1, len(next(iter(traces.values()))), step)
    stack = np.vstack([traces[d][idx] for d in FAMILY_DECIMS])
    return float(np.max(stack.max(axis=0) - stack.min(axis=0)))


t_in, fam_in = step_family(BW_IN)
t_out, fam_out = step_family(BW_OUT)
spread_in = worst_spread(fam_in)
spread_out = worst_spread(fam_out)

print()
print("=== decim neutrality vs 4*decim*loop_bw ===")
print(
    f"  group {GROUP_IN:<5} (loop_bw {BW_IN:.2e}): "
    f"worst spread {spread_in:.3f} dB"
)
print(
    f"  group {GROUP_OUT:<5} (loop_bw {BW_OUT:.2e}): "
    f"worst spread {spread_out:.3f} dB"
)

# The rule is the claim this example exists to demonstrate, so it is
# asserted rather than only drawn. These families cold-start into a weak
# input, so the loop must RAISE its gain — the worse of the two directions
# (the detector is inside the loop and measures power), and the one the
# 0.3 dB promise is set by. An earlier draft of the rule was calibrated on
# the falling direction alone and this assertion is what caught it.
assert spread_in < 0.3, (
    f"inside the rule (4*decim*loop_bw = {GROUP_IN}) the family spread "
    f"{spread_in:.3f} dB, over the 0.3 dB the rule promises"
)
assert spread_out > 3 * spread_in, (
    f"outside the rule the family spread {spread_out:.3f} dB is not "
    f"meaningfully worse than inside it ({spread_in:.3f} dB) — the panel "
    f"would show no contrast"
)

fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(9, 8.5))

ax1.axhline(
    REF_DB, color="0.6", ls="--", lw=1, label=f"reference ({REF_DB:.0f} dB)"
)
ax1.plot(n, in_db, color="tab:orange", label="input power")
colors = ("tab:blue", "tab:green", "tab:red")
for d, color in zip(DECIMS, colors):
    ax1.plot(n, runs[d][0], color=color, lw=1, label=f"output (decim {d})")
ax1.axvline(N_STEP, color="0.6", ls=":", lw=1)
ax1.set_ylabel("power (dB)")
ax1.set_title("AGC decimated loop: decim 1 / 8 / 16 at one loop bandwidth")
ax1.legend(loc="center right", fontsize=8)
ax1.grid(alpha=0.3)

for d, color in zip(DECIMS, colors):
    ax2.plot(
        n, runs[d][1], color=color, lw=1, label=f"applied gain (decim {d})"
    )
ax2.axvline(N_STEP, color="0.6", ls=":", lw=1)
ax2.set_xlabel("sample")
ax2.set_ylabel("applied gain (dB)")
ax2.legend(loc="center right", fontsize=8)
ax2.grid(alpha=0.3)

# Panel 3 — the family. Solid = inside the rule (curves land on top of one
# another), dashed = six times the rule (they fan out). Same three decims
# in both, same colours as the panels above.
for d, color in zip(FAMILY_DECIMS, colors):
    ax3.plot(t_in, fam_in[d], color=color, lw=1.4, label=f"decim {d}")
for d, color in zip(FAMILY_DECIMS, colors):
    ax3.plot(t_out, fam_out[d], color=color, lw=1.4, ls="--")
ax3.set_xlabel("time (loop time constants, $1/4B_L$)")
ax3.set_ylabel("applied gain (dB)")
ax3.set_title(
    f"Step-response family (cold start, weak input) — solid: "
    f"$4\\,d\\,B_L$ = {GROUP_IN} "
    f"(spread {spread_in:.3f} dB)   "
    f"dashed: {GROUP_OUT} (spread {spread_out:.2f} dB)"
)
ax3.legend(loc="lower right", fontsize=8, title="both line styles")
ax3.grid(alpha=0.3)
ax3.annotate(
    "keep $4\\,d\\,B_L \\leq 0.05$\nand decim costs < 0.1 dB",
    xy=(0.02, 0.06),
    xycoords="axes fraction",
    fontsize=8,
    bbox={"boxstyle": "round", "fc": "white", "ec": "0.7", "alpha": 0.9},
)

fig.tight_layout()
out_path = "agc_convergence.png"
fig.savefig(out_path, dpi=120)
print(f"wrote {out_path}")
