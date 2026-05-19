"""agc_demo.py — AGC decimated-loop convergence demo with plots.

Feeds the AGC a constant-envelope tone whose power steps by 20 dB partway
through, and runs the same input through three decimation settings —
decim = 1, 8, 16 — at one fixed loop bandwidth.

agc_steps() runs the detector + loop filter once per chunk of `decim`
samples, but rescales the per-chunk coefficients from `loop_bw` / `alpha`
(k_c = c * 4 * loop_bw, alpha_c = 1 - (1 - alpha)**c).  That rescaling is
what keeps `loop_bw` its per-sample meaning, so all three decim settings
share one effective loop bandwidth and converge on top of each other —
decim only coarsens the path, not the destination.

Saves a two-panel plot to agc_convergence.png:
  - top    : input vs output power (dB) for each decim, with the reference
  - bottom : applied gain (dB) for each decim — the gain actually seen by
             each sample.  agc_steps() commands a new gain once per chunk
             but applies it as a first-order hold, ramping linearly across
             the chunk, so the trace is smooth rather than a staircase.

Run:
  python examples/python/agc_demo.py
"""

import matplotlib

matplotlib.use("Agg")  # headless: render straight to a file, no display

import matplotlib.pyplot as plt
import numpy as np

from doppler.agc import AGC

N_TOTAL = 6000  # total samples processed
N_STEP = 3000  # sample index where the input level jumps
F_TONE = 0.02  # normalised tone frequency (cycles/sample)
REF_DB = 0.0  # AGC target output power
LOOP_BW = 0.00125  # loop noise bandwidth, cycles/sample (fixed for all decim)
ALPHA = 0.02  # power-detector EMA coefficient
LO_DB = -10.0  # input power before the step
HI_DB = 10.0  # input power after the step
DECIMS = (1, 8, 16)  # decimation factors compared at one loop bandwidth

# Constant-envelope tone whose power steps LO_DB -> HI_DB at sample N_STEP.
n = np.arange(N_TOTAL)
amp = np.where(n < N_STEP, 10.0 ** (LO_DB / 20.0), 10.0 ** (HI_DB / 20.0))
x = (amp * np.exp(2j * np.pi * F_TONE * n)).astype(np.complex64)


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
    print(f"decim {d:2d}: settles within 1 dB of {REF_DB:.0f} dB "
          f"{settle} samples after the step; "
          f"applied_gain_db={agc.applied_gain_db:+.2f} "
          f"(commanded gain_db={agc.gain_db:+.2f})")

fig, (ax1, ax2) = plt.subplots(2, 1, sharex=True, figsize=(9, 6))

ax1.axhline(REF_DB, color="0.6", ls="--", lw=1,
            label=f"reference ({REF_DB:.0f} dB)")
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
    ax2.plot(n, runs[d][1], color=color, lw=1,
             label=f"applied gain (decim {d})")
ax2.axvline(N_STEP, color="0.6", ls=":", lw=1)
ax2.set_xlabel("sample")
ax2.set_ylabel("applied gain (dB)")
ax2.legend(loc="center right", fontsize=8)
ax2.grid(alpha=0.3)

fig.tight_layout()
out_path = "agc_convergence.png"
fig.savefig(out_path, dpi=120)
print(f"wrote {out_path}")
