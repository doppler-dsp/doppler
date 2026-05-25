"""detection2d_demo.py — 2-D acquisition grid with theory-driven dwell and CFAR.

Frames the Detector2D as a GPS/CDMA acquisition search: the correlation
surface is an N_DOPPLER × N_CODE_PHASE grid where each cell is an
independent matched-filter hypothesis testing a specific (Doppler bin,
code-phase offset) pair.  The FFT2 correlator evaluates all N cells
simultaneously in O(N log N) instead of O(N²).

Physical interpretation
-----------------------
- **Column axis (code phase)**: integer chip offset from the locally
  generated PRN replica.  The true signal arrives at ``CODE_PHASE_TRUE``
  chips of delay.
- **Row axis (Doppler bin)**: residual carrier frequency offset,
  expressed as a bin index into the search grid.  The true signal sits
  at ``DOPPLER_BIN_TRUE``.
- Each cell ``(i, j)`` asks: "does the received signal match a PRN
  delayed by ``j`` chips and Doppler-shifted by bin ``i``?"  A cell
  fires (detection) when its matched-filter output exceeds the CFAR gate.

Corr2D evaluates all N = N_DOPPLER × N_CODE_PHASE cells in one FFT2
call — the output is the full acquisition surface.

Detection theory — N-cell Bonferroni correction
-------------------------------------------------
Detector2D searches all N cells and returns the maximum.  Each cell is
an independent Rayleigh test under H0, so the system false-alarm rate is:

    Pfa_sys = 1 − (1 − pfa_cell)^N   ≈   N · pfa_cell   (small pfa_cell)

To hit a system Pfa target, first invert for the per-cell rate:

    pfa_cell = 1 − (1 − Pfa_sys)^(1/N)

det_threshold(pfa_cell) returns η_cell (Marcum Q argument).  The gate on
test_stat = peak_mag / noise_est is:

    θ = η_cell · √(2 / π)             (Rayleigh Pfa = exp(−θ²·π/4))

det_dwell() and det_pd() are called with pfa_cell and η_cell to find the
minimum dwell and predict the single-cell Pd.  The system Pd is:

    Pd_sys = 1 − (1 − Pd_cell) · (1 − pfa_cell)^(N−1)

Three panels
------------
Left   — Acquisition surface |R[i,j]| after M coherent integrations:
         code phase on x-axis, Doppler bin on y-axis.  White cross marks
         the injected (Doppler, code-phase); red circle marks the peak.
Middle — Pd vs dwell M: Marcum Q theory curve + MC point.  Vertical
         line marks the required dwell.
Right  — ROC curve at operating SNR and dwell: theory line, empirical
         ROC swept across stored test statistics, MC operating point.

Run::

    python examples/python/detection2d_demo.py

Saves detection2d_demo.png.  Runs in ~10 s.
"""

import math

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np

from doppler.detection import det_dwell, det_pd, det_threshold
from doppler.spectral import Corr2D, Detector2D

# ── Search grid ───────────────────────────────────────────────────────────────

N_DOPPLER    = 16   # Doppler search bins  (rows)
N_CODE_PHASE = 16   # code-phase search bins (columns, = PRN length in chips)
N            = N_DOPPLER * N_CODE_PHASE   # total cells

# True signal location in the search grid.
# Flat index 0 → noise reference uses all other N-1=255 cells, maximising
# the reference population and minimising CFAR scalloping loss.
DOPPLER_BIN_TRUE    = 5    # true Doppler bin
CODE_PHASE_BIN_TRUE = 11   # true code-phase offset (chips)

# ── Detection parameters ──────────────────────────────────────────────────────

SNR_DB   = 3.0       # per-sample amplitude SNR (dB)
SIGMA    = 1.0       # noise std dev per real/imag component

PFA      = 1e-3      # false-alarm probability (per cell per dwell)
PD_MIN   = 0.90      # minimum detection probability requirement
MAX_DWELL = 64       # upper search limit for det_dwell()

N_TRIALS = 3_000     # MC trials
RNG      = np.random.default_rng(0)

# ── Theory: minimum dwell and CFAR threshold ──────────────────────────────────

snr_amp = 10.0 ** (SNR_DB / 20.0)

# Bonferroni correction: N cells tested per dwell → per-cell Pfa must be
# much tighter so that the system (max-of-N) Pfa meets the target.
pfa_cell = 1.0 - (1.0 - PFA) ** (1.0 / N)

# det_threshold() returns η (Marcum Q argument).
# The gate on test_stat = peak_mag / noise_est is θ = η · √(2/π).
eta   = det_threshold(pfa_cell)
theta = eta * math.sqrt(2.0 / math.pi)

M = det_dwell(snr_amp, PD_MIN, pfa_cell, MAX_DWELL)
if M <= 0:
    raise RuntimeError(
        f"SNR = {SNR_DB} dB is not achievable at Pd ≥ {PD_MIN}, "
        f"Pfa_sys = {PFA:.0e} within {MAX_DWELL} dwells."
    )

pd_cell   = det_pd(snr_amp, M, eta)
pd_theory = 1.0 - (1.0 - pd_cell) * (1.0 - pfa_cell) ** (N - 1)

print(f"Grid            : {N_DOPPLER} Doppler × {N_CODE_PHASE} code-phase  "
      f"(N = {N} cells)")
print(f"Amplitude SNR   : {SNR_DB:+.1f} dB  (linear {snr_amp:.3f})")
print(f"Pfa_sys target  : {PFA:.0e}  →  pfa_cell = {pfa_cell:.2e}")
print(f"                        →  η = {eta:.4f},  θ = {theta:.4f}")
print(f"Pd target       : {PD_MIN:.2f}   →  required dwell M = {M}")
print(f"Theory Pd @ M   : {pd_theory:.4f}  (cell: {pd_cell:.4f})")

# ── Reference template ────────────────────────────────────────────────────────
# Build a CAZAC-style (flat-spectrum) reference by assigning a random phase
# to each 2D FFT bin and synthesizing via IFFT.  Flat spectrum → exactly
# zero circular autocorrelation outside lag (0,0).  This prevents the
# coherent ±1 BPSK sidelobe contamination that would otherwise inflate the
# CFAR noise estimate over M dwells and cause a systematic ~3% Pd loss.
#
# In a GPS/CDMA receiver the 2D ref template encodes each (Doppler, code-
# phase) hypothesis; using a flat-power-spectrum template is equivalent to
# an acquisition grid with orthogonal hypotheses (like a CAZAC / Zadoff-Chu).

phases_spec = RNG.uniform(0, 2.0 * np.pi, (N_DOPPLER, N_CODE_PHASE)).astype(
    np.float32
)
# Unit-amplitude spectrum → IFFT synthesizes the flat-autocorrelation template.
# numpy ifft2 normalises by 1/N, giving ||ref2d||² = 1; we scale by sqrt(N) so
# that the matched-filter peak (= ||ref2d||² × N) equals N, matching the SNR
# formula A = snr · σ / √N (signal per frame at true cell = A·N).
ref2d = (np.sqrt(N) * np.fft.ifft2(np.exp(1j * phases_spec))).astype(
    np.complex64
)

# Correlator normalizes by N per frame, so the per-frame amplitude at the
# true cell equals A exactly.  The per-component noise at the output is
# σ / √(2N) per frame.  The amplitude SNR seen by det_pd is therefore:
#     snr = A · √N / σ   →   A = snr · σ / √N
A    = snr_amp * SIGMA / math.sqrt(N)
_ns  = np.float32(SIGMA / math.sqrt(2.0))   # per-component noise std


def signal_frame() -> np.ndarray:
    """One CF32 acquisition frame with signal at (DOPPLER_BIN_TRUE, CODE_PHASE_BIN_TRUE)."""
    sig = np.roll(
        np.roll(ref2d * A, DOPPLER_BIN_TRUE, axis=0),
        CODE_PHASE_BIN_TRUE, axis=1,
    )
    noise = (
        RNG.standard_normal((N_DOPPLER, N_CODE_PHASE))
        + 1j * RNG.standard_normal((N_DOPPLER, N_CODE_PHASE))
    ).astype(np.complex64) * _ns
    return sig + noise


def noise_frame() -> np.ndarray:
    """One CF32 frame: AWGN only (no signal)."""
    return (
        RNG.standard_normal((N_DOPPLER, N_CODE_PHASE))
        + 1j * RNG.standard_normal((N_DOPPLER, N_CODE_PHASE))
    ).astype(np.complex64) * _ns


# ── Acquisition surface (one trial, for the heatmap) ─────────────────────────

with Corr2D(ref2d, dwell=M) as c:
    surf = None
    for _ in range(M):
        out = c.execute(signal_frame())
        if out is not None:
            surf = np.abs(out).reshape(N_DOPPLER, N_CODE_PHASE)

peak_doppler, peak_code_phase = np.unravel_index(
    surf.argmax(), (N_DOPPLER, N_CODE_PHASE)
)

# ── Monte Carlo ───────────────────────────────────────────────────────────────
# threshold=0 → always fire; apply θ in Python to build the ROC.
#
# Guard band: exclude the signal cell from the noise reference (flat index
# = DOPPLER_BIN_TRUE * N_CODE_PHASE + CODE_PHASE_BIN_TRUE), analogous to
# noise_lo = LAG + guard in the 1-D Detector.  Without this, the signal
# cell inflates noise_est and raises the effective threshold.
# Signal flat index = 0; guard one cell so noise_lo=1 excludes the signal.
_signal_flat = DOPPLER_BIN_TRUE * N_CODE_PHASE + CODE_PHASE_BIN_TRUE  # = 0
_noise_lo    = _signal_flat + 1   # 1
_noise_hi    = N - 1              # 255 reference cells (maximum)

det = Detector2D(
    ref2d, dwell=M, noise_lo=_noise_lo, noise_hi=_noise_hi, threshold=0.0
)

print(f"\nRunning {N_TRIALS:,} signal trials…")
sig_stats = np.empty(N_TRIALS)
for i in range(N_TRIALS):
    block = np.concatenate([signal_frame().ravel() for _ in range(M)])
    for *_, stat in det.push(block):
        sig_stats[i] = stat

print(f"Running {N_TRIALS:,} noise trials…")
noise_stats = np.empty(N_TRIALS)
for i in range(N_TRIALS):
    block = np.concatenate([noise_frame().ravel() for _ in range(M)])
    for *_, stat in det.push(block):
        noise_stats[i] = stat

pd_mc  = float((sig_stats   > theta).mean())
pfa_mc = float((noise_stats > theta).mean())

print(f"\nMC Pd  = {pd_mc:.4f}  (theory {pd_theory:.4f})")
print(f"MC Pfa = {pfa_mc:.4f}  (target  {PFA:.4f})")

# ── Theory curves ─────────────────────────────────────────────────────────────

dwell_x     = np.arange(1, min(MAX_DWELL, M * 3) + 1, dtype=int)
pd_vs_dwell = np.array([
    1.0 - (1.0 - det_pd(snr_amp, int(d), eta)) * (1.0 - pfa_cell) ** (N - 1)
    for d in dwell_x
])

# ROC: sweep system Pfa; derive per-cell Pfa and threshold at each point.
pfa_sweep  = np.logspace(-5, 0, 300)
pd_roc_th  = np.array([
    1.0 - (
        1.0 - det_pd(snr_amp, M,
                     det_threshold(1.0 - (1.0 - float(p)) ** (1.0 / N)))
    ) * (1.0 - (1.0 - (1.0 - float(p)) ** (1.0 / N))) ** (N - 1)
    for p in pfa_sweep
])

thresholds = np.percentile(
    np.concatenate([sig_stats, noise_stats]), np.linspace(0, 100, 500)
)
roc_pfa = np.array([(noise_stats > t).mean() for t in thresholds])
roc_pd  = np.array([(sig_stats  > t).mean() for t in thresholds])

# ── Plot ──────────────────────────────────────────────────────────────────────

fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(14, 4.5))
fig.suptitle(
    rf"2-D acquisition  ({N_DOPPLER} Doppler × {N_CODE_PHASE} code-phase,"
    rf"  SNR = {SNR_DB:+.0f} dB,  $P_{{fa}}$ = {PFA:.0e},"
    rf"  required dwell $M$ = {M})",
    fontsize=10,
)

# Panel 1 — acquisition surface
im = ax1.imshow(surf, origin="upper", cmap="viridis", aspect="equal")
ax1.plot(
    CODE_PHASE_BIN_TRUE, DOPPLER_BIN_TRUE,
    "w+", ms=14, mew=2, label="true cell",
)
ax1.plot(
    peak_code_phase, peak_doppler,
    "ro", ms=9, mfc="none", mew=1.8,
    label=f"peak (D={peak_doppler}, CP={peak_code_phase})",
)
ax1.set_xlabel("code-phase bin")
ax1.set_ylabel("Doppler bin")
ax1.set_title(rf"Acquisition surface $|R[i,j]|$ after $M$={M}")
ax1.legend(fontsize=8, loc="lower right")
fig.colorbar(im, ax=ax1, fraction=0.046)

# Panel 2 — Pd vs dwell
ax2.plot(dwell_x, pd_vs_dwell, color="C0", lw=2, label="Theory")
ax2.axhline(PD_MIN, color="0.5", ls="--", lw=0.9)
ax2.text(
    dwell_x[-1] * 0.98, PD_MIN + 0.02,
    rf"$P_d = {PD_MIN}$", ha="right", va="bottom", color="0.4", fontsize=9,
)
ax2.plot(M, pd_mc, "o", color="C1", ms=8, zorder=5,
         label=f"MC ({N_TRIALS:,} trials)")
ax2.axvline(M, color="0.7", ls=":", lw=1)
ax2.text(M + 0.3, 0.05, f"M={M}", color="0.4", fontsize=8.5)
ax2.set_xlim(1, dwell_x[-1])
ax2.set_ylim(-0.02, 1.05)
ax2.set_xlabel("Dwell M (coherent integrations)", fontsize=10)
ax2.set_ylabel(r"$P_d$", fontsize=10)
ax2.set_title(
    rf"$P_d$ vs dwell  (SNR = {SNR_DB:+.0f} dB,  $P_{{fa}}$ = {PFA:.0e})",
    fontsize=10,
)
ax2.legend(fontsize=9)
ax2.grid(True, ls=":", lw=0.6, alpha=0.8)

# Panel 3 — ROC
ax3.plot(pfa_sweep, pd_roc_th, color="C0", lw=2, label="Theory")
ax3.plot(roc_pfa, roc_pd, color="C0", alpha=0.25, lw=1.2,
         label="MC (swept threshold)")
ax3.plot(pfa_mc, pd_mc, "o", color="C1", ms=8, zorder=5,
         label=rf"Operating point ($P_{{d,\mathrm{{sys}}}}$={pd_mc:.3f})")
ax3.axvline(PFA,    color="0.6", ls=":", lw=1)
ax3.axhline(PD_MIN, color="0.6", ls=":", lw=1)
ax3.set_xscale("log")
ax3.set_xlim(1e-5, 1.0)
ax3.set_ylim(-0.02, 1.05)
ax3.set_xlabel(r"$P_{fa}$", fontsize=10)
ax3.set_ylabel(r"$P_d$", fontsize=10)
ax3.set_title(rf"ROC  ($M$={M}, SNR = {SNR_DB:+.0f} dB)", fontsize=10)
ax3.legend(fontsize=9)
ax3.grid(True, which="both", ls=":", lw=0.6, alpha=0.8)

fig.tight_layout()
out = "detection2d_demo.png"
fig.savefig(out, dpi=150, bbox_inches="tight")
print(f"saved {out}")
