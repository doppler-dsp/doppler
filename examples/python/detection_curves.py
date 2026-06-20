"""detection_curves.py — Pd vs dwell and required dwell vs SNR.

Visualises doppler.detection theory functions at Pfa = 1e-5.

  Left  — Pd vs dwell for SNR = 0, 3, 6, 10 dB.
           A dashed horizontal line marks the Pd = 0.9 target.  Filled
           circles annotate the minimum dwell that first crosses it.

  Right — Minimum required dwell to achieve Pd = 0.9 vs per-sample
           amplitude SNR.  Shows how coherent integration gain compensates
           for low SNR: dwell scales roughly as 1 / SNR^2.

Run::

    python examples/python/detection_curves.py

Saves detection_curves.png in the working directory.
"""

import matplotlib

matplotlib.use("Agg")  # headless — no display required

import matplotlib.pyplot as plt
import numpy as np

from doppler.detection import det_dwell, det_pd, det_threshold

# ── Parameters ───────────────────────────────────────────────────────────────

PFA = 1e-5
PD_TARGET = 0.9
SNR_DB = [0, 3, 6, 10]  # curves for left panel
MAX_DWELL = 64
DWELL_X = np.arange(1, MAX_DWELL + 1)

ETA = det_threshold(PFA)  # threshold is Pfa-only; computed once

# ── Compute curves ───────────────────────────────────────────────────────────

# Left panel: Pd vs dwell for each SNR.
snr_amps = [10 ** (db / 20) for db in SNR_DB]
pd_curves = [[det_pd(snr, int(d), ETA) for d in DWELL_X] for snr in snr_amps]

# Right panel: minimum dwell achieving Pd = 0.9 vs SNR from -3 to 15 dB.
snr_db_sweep = np.linspace(-3, 15, 300)
snr_amp_sweep = 10 ** (snr_db_sweep / 20)
min_dwell = [
    det_dwell(float(s), PD_TARGET, PFA, MAX_DWELL) for s in snr_amp_sweep
]

# ── Plot ─────────────────────────────────────────────────────────────────────

COLORS = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728"]

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11, 4.5))
fig.suptitle(
    rf"Detection theory — amplitude-ratio test, $P_{{fa}} = {PFA:.0e}$",
    fontsize=12,
)

# ── Left panel ───────────────────────────────────────────────────────────────

ax1.axhline(PD_TARGET, color="0.5", linestyle="--", linewidth=0.9, zorder=1)
ax1.text(
    MAX_DWELL * 0.98,
    PD_TARGET + 0.02,
    rf"$P_d = {PD_TARGET}$",
    ha="right",
    va="bottom",
    color="0.4",
    fontsize=9,
)

for i, (snr_db, pds) in enumerate(zip(SNR_DB, pd_curves)):
    label = rf"SNR = {snr_db:+d} dB"
    ax1.plot(DWELL_X, pds, color=COLORS[i], linewidth=1.8, label=label)

    # Annotate the crossing point.
    crossing = next(
        (int(d) for d, p in zip(DWELL_X, pds) if p >= PD_TARGET), None
    )
    if crossing is not None:
        pd_cross = det_pd(10 ** (snr_db / 20), crossing, ETA)
        ax1.plot(
            crossing, pd_cross, "o", color=COLORS[i], markersize=6, zorder=5
        )
        # Stagger annotations vertically so high-SNR labels don't collide.
        y_offset = -0.06 - i * 0.07
        ax1.annotate(
            f"M={crossing}",
            xy=(crossing, pd_cross),
            xytext=(crossing + 0.8, pd_cross + y_offset),
            fontsize=7.5,
            color=COLORS[i],
        )

ax1.set_xlim(1, MAX_DWELL)
ax1.set_ylim(-0.02, 1.05)
ax1.set_xlabel("Dwell M (coherent integrations)", fontsize=10)
ax1.set_ylabel(
    r"$P_d = Q_1\!\left(\sqrt{2M}\cdot\mathrm{SNR},\;\eta\right)$", fontsize=10
)
ax1.set_title(r"$P_d$ vs dwell", fontsize=11)
ax1.legend(fontsize=9, loc="lower right")
ax1.grid(True, linestyle=":", linewidth=0.6, alpha=0.8)

# ── Right panel ──────────────────────────────────────────────────────────────

# Mask SNRs where det_dwell returned -1 (not achievable within MAX_DWELL).
valid = np.array(min_dwell)
mask = valid > 0

ax2.semilogy(
    snr_db_sweep[mask],
    valid[mask],
    color="#1f77b4",
    linewidth=1.8,
)

# Annotate the four SNR values from the left panel.
for snr_db, snr_amp, color in zip(SNR_DB, snr_amps, COLORS):
    m = det_dwell(snr_amp, PD_TARGET, PFA, MAX_DWELL)
    if m > 0:
        ax2.plot(
            snr_db,
            m,
            "o",
            color=color,
            markersize=7,
            zorder=5,
            label=rf"SNR = {snr_db:+d} dB → M={m}",
        )
        ax2.annotate(
            f"M={m}",
            xy=(snr_db, m),
            xytext=(snr_db + 0.3, m * 1.25),
            fontsize=8,
            color=color,
        )

ax2.set_xlim(-3, 15)
ax2.set_ylim(0.8, MAX_DWELL * 1.5)
ax2.set_xlabel("Per-sample amplitude SNR (dB)", fontsize=10)
ax2.set_ylabel("Minimum dwell M", fontsize=10)
ax2.set_title(
    rf"Min dwell for $P_d \geq {PD_TARGET}$, $P_{{fa}} = {PFA:.0e}$",
    fontsize=11,
)
ax2.legend(fontsize=8, loc="upper right")
ax2.grid(True, which="both", linestyle=":", linewidth=0.6, alpha=0.8)

# ── Save ─────────────────────────────────────────────────────────────────────

fig.tight_layout()
out = "detection_curves.png"
fig.savefig(out, dpi=150, bbox_inches="tight")
print(f"saved {out}")
