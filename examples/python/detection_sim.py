"""detection_sim.py — Monte Carlo simulation vs detection theory.

Validates both the envelope and power detectors against Marcum Q predictions
from doppler.detection.

Signal model
------------
A complex tone occupies one frame of N samples:

    ref[k] = exp(j·2π·k/N),  k = 0, …, N−1

The FFT correlator normalised by N produces at lag 0:

    R[0] = A + noise_at_output,   noise_at_output ~ CN(0, 2σ²/N)

After M coherent integrations (dwell=M):

    R_acc[0] ~ Rice(A·M, σ_out)   (H1)
    R_acc[0] ~ CN(0, 2Mσ²/N)      (H0)

where σ_out = σ·√(M/N) is the per-component noise std at the correlator
output.

SNR definition
--------------
The post-correlation amplitude SNR is:

    snr = A · √(N/2) / σ     (dimensionless)

so the tone power in SNR units is snr² (power SNR).  Given a target snr, the
corresponding input amplitude is:

    A = snr · σ · √(2/N)

Envelope detector (Case 1)
--------------------------
Test statistic:

    env_stat = |R_acc[0]| / mean(|R_acc[τ]|, τ=1…N−1)

The noise estimator mean(|R_acc[τ]|) converges to σ_out·√(π/2), so under H0:

    env_stat ~ Rayleigh(√(2/π)),  Pfa = exp(−θ²·π/4)

Threshold: θ = √(−4·ln Pfa / π) = det_threshold(Pfa) · √(2/π)

Detection probability:

    Pd = Q₁(√(2M)·snr, det_threshold(Pfa))   (Marcum Q, 1st order)

Power detector (Case 2)
-----------------------
Test statistic:

    pow_stat = |R_acc[0]|² / mean(|R_acc[τ]|², τ=1…N−1)

The noise estimator mean(|R_acc[τ]|²) converges to 2Mσ²/N = 2σ_out², so
under H0:

    pow_stat ~ Exponential(1),   Pfa = exp(−p)

Threshold: p = −ln(Pfa)  (simpler than the envelope case; no π factor)

Detection probability at amplitude SNR snr (or power SNR snr_power = snr²):

    Pd = Q₁(√(2·M·snr_power), √(2p))

    = det_pd_power(snr_power, M, p)   with snr_power = snr²

Equivalence: det_pd_power(snr², M, p) == det_pd(snr, M, eta)
             because √(2p) = det_threshold(Pfa) = eta.

Run::

    python examples/python/detection_sim.py

Saves detection_sim.png.  Runs in ~5 s.
"""

import math

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np

from doppler.detection import (
    det_pd,
    det_pd_power,
    det_threshold,
    det_threshold_power,
    marcum_q,
)

# ── Simulation parameters ─────────────────────────────────────────────────────

N = 64  # frame length (complex samples)
DWELL = 4  # coherent integrations
SIGMA = 1.0  # noise std dev per real/imag component
PFA = 1e-2  # false-alarm rate (moderate for MC statistics)
N_TRIALS = 30_000  # trials per SNR point
SNR_HIST = 1.5  # SNR used for the survival-function panels

# Envelope detector thresholds.
ETA = det_threshold(PFA)  # Q₁ argument
THETA = ETA * math.sqrt(2.0 / math.pi)  # env_stat threshold

# Power detector thresholds.
P_THRESH = det_threshold_power(PFA)  # pow_stat threshold = -ln(Pfa)

RNG = np.random.default_rng(0)


# ── Monte Carlo core ──────────────────────────────────────────────────────────


def simulate(snr: float, n_trials: int = N_TRIALS) -> tuple:
    """Run Monte Carlo trials and return both test statistics.

    Samples the matched-filter output and noise reference directly from
    their theoretical distributions.  The FFT cross-correlation of a
    single-bin tone with AWGN produces R[τ] = W·exp(j2πτ/N), making
    |R[τ]| identical at every lag — so both test statistics would be 1
    regardless of SNR.  Direct sampling avoids that degeneracy and is
    the canonical way to verify the Marcum Q model.

    After M coherent integrations:
    - Signal channel: R₀ ~ CN(M·A, 2·M·σ²/N)
    - Noise reference: N−1 i.i.d. samples from CN(0, 2·M·σ²/N)

    Parameters
    ----------
    snr : float
        Post-correlation amplitude SNR: snr = A·√(N/2)/σ.  Zero → H0.
    n_trials : int
        Number of independent trials.

    Returns
    -------
    env_stat : np.ndarray, shape (n_trials,)
        |R₀| / mean(|noise_ref|).
    pow_stat : np.ndarray, shape (n_trials,)
        |R₀|² / mean(|noise_ref|²).
    """
    A = snr * SIGMA * math.sqrt(2.0 / N)
    sig_amp = DWELL * A  # coherently accumulated
    sigma_ac = SIGMA * math.sqrt(DWELL / N)  # per-component std dev

    # Matched-filter output at lag 0: CN(sig_amp, 2·sigma_ac²).
    re0 = sig_amp + RNG.standard_normal(n_trials) * sigma_ac
    im0 = RNG.standard_normal(n_trials) * sigma_ac
    mag0 = np.hypot(re0, im0)

    # N−1 independent noise reference samples from CN(0, 2·sigma_ac²).
    re_n = RNG.standard_normal((n_trials, N - 1)) * sigma_ac
    im_n = RNG.standard_normal((n_trials, N - 1)) * sigma_ac
    mag_n = np.hypot(re_n, im_n)

    env_stat = mag0 / mag_n.mean(axis=1)
    pow_stat = mag0**2 / (mag_n**2).mean(axis=1)
    return env_stat, pow_stat


# ── Theory survival functions ─────────────────────────────────────────────────


def env_sf(snr: float, x: np.ndarray) -> np.ndarray:
    """P(env_stat > x): Q₁(√(2M)·snr, x·√(π/2))."""
    a = math.sqrt(2.0 * DWELL) * snr
    return np.array(
        [marcum_q(1, a, float(xi) * math.sqrt(math.pi / 2)) for xi in x]
    )


def pow_sf(snr: float, x: np.ndarray) -> np.ndarray:
    """P(pow_stat > x): Q₁(√(2M·snr²), √(2x)) = det_pd_power(snr², M, x)."""
    return np.array([det_pd_power(snr**2, DWELL, float(xi)) for xi in x])


# ── Run simulations ───────────────────────────────────────────────────────────

print(f"Running H0 ({N_TRIALS:,} trials)…")
env_h0, pow_h0 = simulate(0.0)

print(f"Running H1 snr={SNR_HIST} ({N_TRIALS:,} trials)…")
env_h1, pow_h1 = simulate(SNR_HIST)

snr_sweep = np.linspace(0.0, 3.0, 25)
print(f"Running Pd sweep ({len(snr_sweep)} SNR × {N_TRIALS:,} trials)…")
pd_env_mc = np.empty(len(snr_sweep))
pd_pow_mc = np.empty(len(snr_sweep))
for i, s in enumerate(snr_sweep):
    ev, pw = simulate(float(s))
    pd_env_mc[i] = (ev > THETA).mean()
    pd_pow_mc[i] = (pw > P_THRESH).mean()

pd_env_th = np.array([det_pd(float(s), DWELL, ETA) for s in snr_sweep])
pd_pow_th = np.array(
    [det_pd_power(float(s) ** 2, DWELL, P_THRESH) for s in snr_sweep]
)

print("Done.")

# ── Plot — 2×2 grid ───────────────────────────────────────────────────────────

fig, axes = plt.subplots(2, 2, figsize=(14, 9))
fig.suptitle(
    rf"Envelope vs Power detector — Monte Carlo vs theory"
    rf"  ($N={N}$, dwell$={DWELL}$, $P_{{fa}}={PFA:.0e}$)",
    fontsize=13,
)

BLUE = "C0"
RED = "C1"

for row, (
    label,
    ts_h0,
    ts_h1,
    sf_fn,
    thresh,
    pd_mc,
    pd_th,
    ax_sf,
    ax_pd,
    x_lim,
    x_lab,
    stat_name,
) in enumerate(
    [
        (
            "Case 1: Envelope",
            env_h0,
            env_h1,
            env_sf,
            THETA,
            pd_env_mc,
            pd_env_th,
            axes[0, 0],
            axes[0, 1],
            8.0,
            r"env_stat = $|R_\mathrm{acc}[0]|\,/\,\mathrm{mean}(|R_\mathrm{acc}[\tau]|)$",
            "env_stat",
        ),
        (
            "Case 2: Power",
            pow_h0,
            pow_h1,
            pow_sf,
            P_THRESH,
            pd_pow_mc,
            pd_pow_th,
            axes[1, 0],
            axes[1, 1],
            25.0,
            r"pow_stat = $|R_\mathrm{acc}[0]|^2\,/\,\mathrm{mean}(|R_\mathrm{acc}[\tau]|^2)$",
            "pow_stat",
        ),
    ]
):
    # ── Survival functions ────────────────────────────────────────────────
    x_plot = np.linspace(0.05, x_lim, 400)

    for ts, snr, color, lbl in [
        (ts_h0, 0.0, BLUE, "H0 (noise only)"),
        (ts_h1, SNR_HIST, RED, rf"H1 (snr={SNR_HIST})"),
    ]:
        ts_s = np.sort(ts)
        sf_e = 1.0 - np.arange(1, len(ts) + 1) / len(ts)
        ax_sf.semilogy(
            ts_s,
            sf_e,
            color=color,
            alpha=0.35,
            linewidth=1.2,
            label=f"{lbl} — MC",
        )
        ax_sf.semilogy(
            x_plot,
            sf_fn(snr, x_plot),
            color=color,
            linewidth=2.0,
            linestyle="--",
            label=f"{lbl} — theory",
        )

    ax_sf.axvline(thresh, color="0.35", linestyle=":", linewidth=1.5)
    ax_sf.text(
        thresh + x_lim * 0.02,
        3e-4,
        rf"$\tau={thresh:.2f}$",
        va="bottom",
        fontsize=9,
        color="0.3",
    )

    pfa_mc = (ts_h0 > thresh).mean()
    pd_mc_pt = (ts_h1 > thresh).mean()
    pd_th_pt = sf_fn(SNR_HIST, np.array([thresh]))[0]

    ax_sf.plot(thresh, PFA, "^", color=BLUE, markersize=8, zorder=6)
    ax_sf.plot(thresh, pd_th_pt, "^", color=RED, markersize=8, zorder=6)
    ax_sf.annotate(
        rf"$P_{{fa}}={pfa_mc:.3f}$",
        xy=(thresh, PFA),
        xytext=(thresh - x_lim * 0.06, PFA * 6),
        fontsize=8.5,
        color=BLUE,
        ha="right",
        arrowprops=dict(arrowstyle="-", color=BLUE, lw=0.8),
    )
    ax_sf.annotate(
        rf"$P_d={pd_mc_pt:.3f}$",
        xy=(thresh, pd_th_pt),
        xytext=(thresh - x_lim * 0.06, pd_th_pt * 0.25),
        fontsize=8.5,
        color=RED,
        ha="right",
        arrowprops=dict(arrowstyle="-", color=RED, lw=0.8),
    )
    ax_sf.set_xlim(0, x_lim)
    ax_sf.set_ylim(1e-4, 1.2)
    ax_sf.set_xlabel(x_lab, fontsize=9)
    ax_sf.set_ylabel(rf"$P(\mathrm{{{stat_name}}} > x)$", fontsize=10)
    ax_sf.set_title(f"{label} — survival functions", fontsize=11)
    ax_sf.legend(fontsize=8, loc="upper right")
    ax_sf.grid(True, which="both", linestyle=":", linewidth=0.6, alpha=0.8)

    # ── Pd vs SNR ─────────────────────────────────────────────────────────
    snr_fine = np.linspace(0.0, 3.0, 200)
    pd_fine = (
        np.array([det_pd(float(s), DWELL, ETA) for s in snr_fine])
        if row == 0
        else np.array(
            [det_pd_power(float(s) ** 2, DWELL, P_THRESH) for s in snr_fine]
        )
    )

    ax_pd.plot(snr_fine, pd_fine, color=BLUE, linewidth=2.0, label="Theory")
    ax_pd.scatter(
        snr_sweep,
        pd_mc,
        color=RED,
        s=30,
        zorder=5,
        label=f"MC ({N_TRIALS:,} trials)",
    )

    ax_pd.axhline(0.9, color="0.5", linestyle="--", linewidth=0.9)
    ax_pd.text(
        snr_fine[-1] * 0.98,
        0.915,
        r"$P_d = 0.9$",
        ha="right",
        va="bottom",
        color="0.4",
        fontsize=9,
    )

    snr_req = next((s for s, p in zip(snr_fine, pd_fine) if p >= 0.9), None)
    if snr_req is not None:
        ax_pd.axvline(snr_req, color="0.6", linestyle=":", linewidth=1.2)
        ax_pd.text(
            snr_req + 0.03,
            0.05,
            f"snr={snr_req:.2f}",
            color="0.4",
            fontsize=8.5,
        )

    ax_pd.set_xlim(0, 3.0)
    ax_pd.set_ylim(-0.02, 1.05)
    ax_pd.set_xlabel(
        r"snr = $A\,\sqrt{N/2}\,/\,\sigma$   (amplitude SNR)",
        fontsize=9,
    )
    ax_pd.set_ylabel(r"$P_d$", fontsize=10)
    ax_pd.set_title(
        rf"{label} — $P_d$ vs SNR (dwell$={DWELL}$, $P_{{fa}}={PFA:.0e}$)",
        fontsize=11,
    )
    ax_pd.legend(fontsize=9, loc="upper left")
    ax_pd.grid(True, linestyle=":", linewidth=0.6, alpha=0.8)

# ── Save ──────────────────────────────────────────────────────────────────────

fig.tight_layout()
out = "detection_sim.png"
fig.savefig(out, dpi=150, bbox_inches="tight")
print(f"saved {out}")
