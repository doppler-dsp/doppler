"""detection_curves.py — Pd vs dwell and required dwell vs SNR.

Visualises doppler.detection theory functions at Pfa = 1e-5.

  Left  — Pd vs dwell for SNR = 0, 3, 6, 10 dB.
           A dashed horizontal line marks the Pd = 0.9 target.  Filled
           circles annotate the minimum dwell that first crosses it.

  Right — Minimum required dwell to achieve Pd = 0.9 vs per-sample
           amplitude SNR.  Shows how coherent integration gain compensates
           for low SNR: dwell scales roughly as 1 / SNR^2.

Run::

    python detection_curves.py

Saves detection_curves.png in the working directory.
"""

import matplotlib

matplotlib.use("Agg")  # headless — no display required

import matplotlib.pyplot as plt

# --8<-- [start:theory]
import numpy as np

from doppler.detection import (
    det_dwell,
    det_pd,
    det_threshold,
    det_threshold_f,
    det_threshold_noncoherent,
)

PFA = 1e-5
PD_TARGET = 0.9
SNR_DB = [0, 3, 6, 10]  # curves for left panel
MAX_DWELL = 64
DWELL_X = np.arange(1, MAX_DWELL + 1)

ETA = det_threshold(PFA)  # threshold is Pfa-only; computed once

# Left panel: Pd vs dwell for each SNR.
snr_amps = [10 ** (db / 20) for db in SNR_DB]
pd_curves = [[det_pd(snr, int(d), ETA) for d in DWELL_X] for snr in snr_amps]

# Right panel: minimum dwell achieving Pd = 0.9 vs SNR from -3 to 15 dB.
snr_db_sweep = np.linspace(-3, 15, 300)
snr_amp_sweep = 10 ** (snr_db_sweep / 20)
min_dwell = [
    det_dwell(float(s), PD_TARGET, PFA, MAX_DWELL) for s in snr_amp_sweep
]

# Mask SNRs where det_dwell returned -1 (not achievable within MAX_DWELL).
valid = np.array(min_dwell)
mask = valid > 0

# Third panel: what an ESTIMATED noise reference costs.
#
# Every curve above prices a statistic normalised by a KNOWN noise power.
# A burst detector has one burst and must estimate the noise from it, so
# the exact H0 law becomes R^2 = n*F(n, n) -- a fatter tail than the
# chi-square gate prices. det_threshold_f is the exact gate; the penalty
# below is what using the chi-square one instead really delivers.
#
# The degrees of freedom are the trap, and the comparator is DERIVED
# rather than picked to match: under H0 the burst's n prompts give
# sum Re^2 ~ s^2*chi2(n), so a caller treating sum Im^2/n as exactly s^2
# believes R^2 ~ chi2(n) and gates at that quantile. Since
# det_threshold_noncoherent(pfa, M) prices chi2(2M), that is M = n/2.
# Pricing it at n gives 4.8x instead of 41x at n=16.
#
# EVEN n only -- n // 2 would price chi2(n-1) otherwise, which is the same
# gate as n-1 and overstates the penalty by about a fifth. doppler cannot
# express an odd-dof known-noise gate; det_threshold_f handles odd n
# deliberately, because a burst's prompt count is whatever it contained.
PFA_BURST = 1e-3
DOFS = [2, 4, 8, 16, 32, 64, 128]
assert all(n % 2 == 0 for n in DOFS)  # see the note above


def realized_pfa(quantile, n):
    """The pfa a given F(n,n) quantile really buys.

    det_threshold_f is monotone DECREASING in pfa, so bisect on the
    geometric mean of the bracket rather than inverting analytically.
    """
    lo, hi = 1e-15, 0.9999
    for _ in range(200):
        mid = float(np.sqrt(lo * hi))
        if det_threshold_f(mid, n) > quantile:
            lo = mid
        else:
            hi = mid
    return float(np.sqrt(lo * hi))


chi_gates = [
    det_threshold_noncoherent(PFA_BURST, max(n // 2, 1)) ** 2 / n for n in DOFS
]
f_gates = [det_threshold_f(PFA_BURST, n) for n in DOFS]
penalty = [realized_pfa(g, n) / PFA_BURST for g, n in zip(chi_gates, DOFS)]

# The TWO prices, and the estimate quality that causes both.
#
# `penalty` is a multiplier on the false-alarm RATE, paid by a detector
# that ignores the estimation. It is NOT "what an estimated reference
# costs" -- quoted as a cost it names no unit. The cost of doing it RIGHT
# is a higher threshold: detection sensitivity, in dB, which is the number
# a link budget wants.
#
# Both are the same sqrt(2/n) seen from two sides. sigma_hat^2 =
# sum Im^2 / n is sigma^2 * chi2(n)/n, whose relative standard deviation
# is sqrt(2/n) -- 35% at n=16. The floor the detector divides by is known
# only to a third, which is why the correct gate must sit so much higher.
sens_db = [10 * np.log10(f / g) for f, g in zip(f_gates, chi_gates)]
est_rel = [np.sqrt(2.0 / n) for n in DOFS]
# --8<-- [end:theory]

# --8<-- [start:checks]
# Self-checks: the theory functions must be internally consistent.
# Coherent integration only helps: Pd is non-decreasing in dwell.
for snr_db, pds in zip(SNR_DB, pd_curves):
    assert np.all(np.diff(pds) >= -1e-12), (
        f"Pd not monotone in dwell at {snr_db} dB"
    )

# det_dwell() must return the *minimum* dwell: Pd first crosses the
# target at M, i.e. Pd(M) >= target and Pd(M-1) < target.
for snr_db, snr_amp in zip(SNR_DB, snr_amps):
    m = det_dwell(snr_amp, PD_TARGET, PFA, MAX_DWELL)
    assert m > 0, f"Pd={PD_TARGET} unreachable at {snr_db} dB"
    assert det_pd(snr_amp, m, ETA) >= PD_TARGET, "det_dwell undershoots"
    assert m == 1 or det_pd(snr_amp, m - 1, ETA) < PD_TARGET, (
        "det_dwell is not minimal"
    )
    print(f"SNR {snr_db:+3d} dB: minimum dwell M = {m}")

# Integration gain compensates SNR: a stronger signal never needs a
# longer dwell, so the right-panel curve is non-increasing.
assert np.all(np.diff(valid[mask]) <= 0), "min dwell not monotone in SNR"

# The estimated-noise penalty is real, large, and shrinks as the estimate
# hardens. 41x at n=16 is the number detection_core.h quotes and the
# certification re-derives two independent ways (analytically here, and
# by 2e6 Monte-Carlo draws in the models characterization).
assert all(f > g for f, g in zip(f_gates, chi_gates)), (
    "the F gate must always be the stricter one"
)
assert abs(penalty[DOFS.index(16)] - 41.0) < 1.0, (
    f"penalty at n=16 is {penalty[DOFS.index(16)]:.1f}x, expected ~41x"
)
assert all(a > b for a, b in zip(penalty, penalty[1:])), (
    "the penalty must shrink monotonically as the noise estimate hardens"
)
assert penalty[-1] > 5.0, (
    "even at n=128 the chi-square gate is still materially mispriced"
)

# Price two: doing it correctly costs SENSITIVITY, not false alarms.
i16 = DOFS.index(16)
assert abs(sens_db[i16] - 3.27) < 0.05, (
    f"the correct gate costs {sens_db[i16]:.2f} dB at n=16, expected 3.27"
)
assert all(a > b for a, b in zip(sens_db, sens_db[1:])), (
    "more prompts in the reference must cost less sensitivity"
)
# Doubling n tightens sqrt(2/n) by sqrt(2) and roughly halves the dB cost.
assert all(0.4 < b / a < 0.75 for a, b in zip(sens_db, sens_db[1:])), (
    "the dB cost should roughly halve per doubling of n"
)
assert abs(est_rel[i16] - 0.3536) < 1e-3, "sqrt(2/16) is 35.4%"

for n, pen, db, rel in zip(DOFS, penalty, sens_db, est_rel):
    print(
        f"n={n:<4} noise known to {rel * 100:5.1f}%  ->  "
        f"ignore it: {pen:5.1f}x the priced pfa   |   "
        f"price it: {db:5.2f} dB of sensitivity"
    )
# --8<-- [end:checks]

# ── Plot ─────────────────────────────────────────────────────────────────────

COLORS = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728"]

fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(16, 4.5))
# The suptitle deliberately does NOT name a law or a Pfa: the first two
# panels are the amplitude-ratio test at PFA, the third is an F(n,n) ratio
# at PFA_BURST, and each panel titles its own. A shared title naming one
# of them would mislabel the other two thirds of the figure.
fig.suptitle(
    "Detection theory — sizing a detector, and what an estimated "
    "noise reference costs",
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

# ── Third panel ──────────────────────────────────────────────────────────────
#
# A ratio and a log axis: the penalty spans 2 decades of dof and only one
# decade of ratio, so both axes are log and the "priced" line is at 1.0.

# Two curves on twin axes, because there are two prices and they are the
# same sqrt(2/n) seen from two sides -- plotting only the multiplier is
# what let "costs 41x" stand as a sentence with no unit in it.
# Tinted to the LEFT axis and labelled on the left: with two y-axes an
# uncoloured reference line reads as the blue curve's asymptote, which it
# is not -- 1.0 is "no mispricing" on the red axis only.
ax3.axhline(
    1.0, color="#d62728", linestyle="--", linewidth=0.9, alpha=0.55, zorder=1
)
ax3.text(
    DOFS[0],
    1.06,
    r"1x = correctly priced",
    ha="left",
    va="bottom",
    color="#d62728",
    alpha=0.8,
    fontsize=8,
)
ax3.loglog(
    DOFS,
    penalty,
    color="#d62728",
    linewidth=1.8,
    marker="o",
    markersize=5,
    label="ignore it: false-alarm RATE multiplier",
)
i_16 = DOFS.index(16)
ax3.annotate(
    f"{penalty[i_16]:.0f}x",
    xy=(16, penalty[i_16]),
    xytext=(16 * 1.35, penalty[i_16] * 1.7),
    fontsize=8.5,
    color="#d62728",
    arrowprops={"arrowstyle": "->", "color": "#d62728", "linewidth": 0.8},
)
ax3.set_xlabel("n (prompts folded into the noise estimate)", fontsize=10)
ax3.set_ylabel(
    r"realized $P_{fa}$ / priced $P_{fa}$", color="#d62728", fontsize=10
)
ax3.tick_params(axis="y", labelcolor="#d62728")
ax3.grid(True, which="both", linestyle=":", linewidth=0.6, alpha=0.8)

ax3b = ax3.twinx()
ax3b.semilogx(
    DOFS,
    sens_db,
    color="#1f77b4",
    linewidth=1.8,
    marker="s",
    markersize=5,
    label="price it: SENSITIVITY given up",
)
ax3b.annotate(
    f"{sens_db[i_16]:.2f} dB",
    xy=(16, sens_db[i_16]),
    xytext=(16 * 1.35, sens_db[i_16] + 1.6),
    fontsize=8.5,
    color="#1f77b4",
    arrowprops={"arrowstyle": "->", "color": "#1f77b4", "linewidth": 0.8},
)
ax3b.set_ylabel("sensitivity given up (dB)", color="#1f77b4", fontsize=10)
ax3b.tick_params(axis="y", labelcolor="#1f77b4")
ax3b.set_ylim(0, max(sens_db) * 1.25)

lines_a, labels_a = ax3.get_legend_handles_labels()
lines_b, labels_b = ax3b.get_legend_handles_labels()
ax3.legend(
    lines_a + lines_b, labels_a + labels_b, fontsize=7.5, loc="upper right"
)
ax3.set_title(
    "Cost of an ESTIMATED noise reference\n"
    rf"(known to {100 * np.sqrt(2 / 16):.0f}% at $n=16$, "
    rf"$P_{{fa}} = {PFA_BURST:.0e}$)",
    fontsize=10.5,
)

# ── Save ─────────────────────────────────────────────────────────────────────

fig.tight_layout()
out = "detection_curves.png"
fig.savefig(out, dpi=150, bbox_inches="tight")
print(f"saved {out}")
