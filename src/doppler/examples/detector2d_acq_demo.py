"""detector2d_acq_demo.py — real DSSS burst acquisition in a noisy Doppler
environment, characterised against SNR.

Scenario
--------
A spread-spectrum transmitter emits a **burst of repeated PN-sequence
segments**, BPSK-modulated and oversampled.  The signal reaches the receiver
with an **unknown integer code phase** (propagation delay) and an **unknown
carrier-frequency offset** (Doppler), buried in additive white Gaussian noise.
The receiver must *acquire* it: jointly estimate ``(Doppler bin, code phase)``
and declare a detection.

How ``Detector2D`` is used
--------------------------
``doppler.spectral.Detector2D`` wraps a 2-D FFT correlator whose surface is

    R = IFFT2( FFT2(x) . conj(FFT2(ref)) ) / (ny*nx)

— an inverse FFT on **both** axes.  That is a 2-D matched filter, *not* by
itself a delay-Doppler search.  We compose it the way a real GPS/DSSS receiver
does — a parallel code-phase correlation on **fast-time** plus a Doppler FFT on
**slow-time**:

1. Frame the received stream into ``(ny, nx)`` where ``nx = sf*sps`` is one PN
   segment (the **code-phase / fast-time** axis) and ``ny`` is the number of
   repeated segments (the **slow-time** axis).
2. Pre-apply ``y = FFT(x, axis=0)`` — the slow-time Doppler FFT.
3. Push ``y`` through ``Detector2D`` with a **single-row** PN reference (the
   oversampled replica in row 0, zeros elsewhere).  The column axis then runs
   the circular code matched filter (peak at the code phase) and the row axis
   is a clean pass-through of the Doppler FFT (peak at the Doppler bin).

A carrier offset ``f`` (cycles/sample) lands the peak at::

    doppler_bin = round(f * nx * ny)  (mod ny)      # bin spacing 1/(nx*ny)
    code_col    = code_phase                        # integer-sample delay

Keep ``|f*nx| < 0.25`` so the within-segment phase rotation costs < ~1 dB of
code-correlation (sinc) loss.

Three panels (saved to ``detector2d_acq_demo.png``)
---------------------------------------------------
1. Acquisition surface ``|R|`` for one high-SNR trial — code phase on x,
   Doppler bin on y; ``+`` at the injected cell, ``o`` at the detected peak.
2. ``Pd`` vs per-sample SNR — Monte-Carlo points with a Marcum-Q theory guide
   and the operating-SNR marker.
3. ROC at the operating SNR — empirical swept-threshold curve, the MC operating
   point, and the Bonferroni system-Pfa target.

Run::

    python -m doppler.examples.detector2d_acq_demo

Runs in ~15 s.
"""

import math

import numpy as np
from numpy.typing import NDArray

from doppler.detection import det_pd, det_threshold
from doppler.spectral import Corr2D, Detector2D
from doppler.wfm import PN, mls_poly

# ── Acquisition grid ─────────────────────────────────────────────────────────

SF = 31  # PN spreading factor (length-5 MLS period = 2^5 - 1)
SPS = 4  # samples per chip (oversampling)
NY = 16  # repeated segments → Doppler search bins (rows)
NX = SF * SPS  # one segment in samples → code-phase bins (columns) = 124
N = NY * NX  # total cells = 1984 → ~33 dB coherent processing gain

PN_POLY = mls_poly(5)  # primitive polynomial for the length-5 MLS
PN_SEED = 1


def make_code(length: int = SF) -> NDArray[np.uint8]:
    """Return one period of the length-5 MLS PN code (0/1 chips).

    Parameters
    ----------
    length : int
        Number of chips to emit; one full period is ``SF`` = 31.

    Returns
    -------
    NDArray[np.uint8]
        ``length`` chips, each 0 or 1.

    Examples
    --------
    >>> code = make_code()
    >>> code.shape, code.dtype
    ((31,), dtype('uint8'))
    >>> int(code.sum())  # MLS has 2^(n-1) = 16 ones per period
    16
    """
    return PN(poly=PN_POLY, seed=PN_SEED, length=5).generate(length)


def build_ref(chips01: NDArray[np.uint8]) -> NDArray[np.complex64]:
    """Build the single-row matched-filter reference.

    The oversampled, undelayed BPSK replica of one PN segment is placed in
    **row 0**; every other row is zero.  ``Detector2D`` FFT2's this internally;
    the flat-in-slow-time row spectrum is what turns the correlator's row axis
    into a pass-through of the pre-applied Doppler FFT (see module docstring).

    Parameters
    ----------
    chips01 : NDArray[np.uint8]
        One PN period (0/1 chips), length ``SF``.

    Returns
    -------
    NDArray[np.complex64]
        ``(NY, NX)`` reference; ``ref[0]`` is the replica, rest zero.

    Examples
    --------
    >>> ref = build_ref(make_code())
    >>> ref.shape, ref.dtype
    ((16, 124), dtype('complex64'))
    >>> bool(np.all(ref[1:] == 0))  # only row 0 carries the replica
    True
    """
    s0 = np.repeat(np.where(chips01 & 1, -1.0, 1.0), SPS).astype(np.complex64)
    ref = np.zeros((NY, NX), dtype=np.complex64)
    ref[0, :] = s0
    return ref


def build_acq_frame(
    rng: np.random.Generator,
    *,
    chips01: NDArray[np.uint8],
    code_phase: int,
    carrier_f: float,
    snr_db: float,
) -> tuple[NDArray[np.complex64], int, int]:
    """Synthesise one received, pre-FFT'd acquisition frame plus ground truth.

    Builds ``ny`` repeated oversampled BPSK PN segments, applies a circular
    code delay and a continuous carrier offset, adds complex AWGN at the target
    per-sample SNR, frames to ``(NY, NX)``, and applies the slow-time Doppler
    FFT (``axis=0``).  The returned array is ready to ``push`` into
    ``Detector2D`` built on :func:`build_ref`.

    Parameters
    ----------
    rng : numpy.random.Generator
        Seeded RNG, for reproducibility.
    chips01 : NDArray[np.uint8]
        One PN period (0/1 chips), length ``SF``.
    code_phase : int
        Integer-sample circular delay in ``[0, NX)``.  The true code column.
    carrier_f : float
        Carrier offset in cycles/sample.  Keep ``|carrier_f*NX| < 0.25`` so the
        Doppler bin stays in the low-loss band ``|u*| < NY/4``.
    snr_db : float
        Per-sample power SNR in dB (signal chip power 1 / complex noise power).

    Returns
    -------
    y : NDArray[np.complex64]
        ``(NY, NX)`` Doppler-FFT'd frame to push into ``Detector2D``.
    doppler_bin : int
        Ground-truth Doppler row, ``round(carrier_f*NX*NY) mod NY``.
    code_col : int
        Ground-truth code column, ``code_phase mod NX``.

    Examples
    --------
    >>> rng = np.random.default_rng(0)
    >>> y, dbin, col = build_acq_frame(
    ...     rng,
    ...     chips01=make_code(),
    ...     code_phase=37,
    ...     carrier_f=2 / (NX * NY),
    ...     snr_db=0.0,
    ... )
    >>> y.shape, y.dtype
    ((16, 124), dtype('complex64'))
    >>> dbin, col
    (2, 37)
    """
    s0 = np.repeat(np.where(chips01 & 1, -1.0, 1.0), SPS).astype(np.complex64)
    s0d = np.roll(s0, code_phase)  # circular delay — matches corr2d wrap
    seg = np.tile(s0d, NY)  # ny identical segments, length N
    sig = seg * np.exp(2j * np.pi * carrier_f * np.arange(N))
    sigma = 10.0 ** (-snr_db / 20.0)  # signal power = 1 → noise std = sigma
    noise = (sigma / math.sqrt(2.0)) * (
        rng.standard_normal(N) + 1j * rng.standard_normal(N)
    )
    x = (sig + noise).astype(np.complex64).reshape(NY, NX)
    y = np.fft.fft(x, axis=0).astype(np.complex64)  # *** slow-time Doppler FFT
    doppler_bin = round(carrier_f * NX * NY) % NY
    return y, doppler_bin, code_phase % NX


def doppler_bins() -> NDArray[np.int_]:
    """Doppler bins exercised by the demo/test — the low-loss band, sans 0."""
    return np.array([-3, -2, -1, 1, 2, 3])


def carrier_for_bin(u: int) -> float:
    """Carrier offset (cyc/sample) that lands the peak on Doppler bin ``u``."""
    return u / (NX * NY)


# ── Monte-Carlo sweep ────────────────────────────────────────────────────────

PFA_SYS = 1e-2  # target system (max-of-N) false-alarm probability
NOISE_MODE = "median"  # robust to the random true-cell and PN sidelobe ridge
NOISE_LO = 0  # CFAR reference band — full surface
NOISE_HI = N - 1  # full surface (== the clamped default; passed for clarity)
OP_SNR = -19.0  # per-sample SNR at the detection transition knee (ROC/op pt)


def make_detector(ref: NDArray[np.complex64]) -> Detector2D:
    """Build the acquisition detector with the canonical CFAR config.

    Passes the full-surface ``[0, N-1]`` noise band explicitly.  (Omitting
    ``noise_hi`` selects the same band via the clamped ``ny*nx-1`` default;
    the explicit form documents intent.)

    Parameters
    ----------
    ref : NDArray[np.complex64]
        Single-row reference from :func:`build_ref`.

    Returns
    -------
    Detector2D
        ``dwell=1``, median CFAR over the full ``[0, N-1]`` surface,
        ``threshold=0`` (gate applied in Python).
    """
    return Detector2D(
        ref,
        dwell=1,
        noise_lo=NOISE_LO,
        noise_hi=NOISE_HI,
        noise_mode=NOISE_MODE,
    )


def _calibrate_theta(
    det: Detector2D, rng: np.random.Generator, n_trials: int
) -> float:
    """Empirical detection gate: the ``1 - PFA_SYS`` quantile of noise-only
    ``test_stat``.  The median noise estimate does not carry the analytic
    Rayleigh scaling, so the gate is calibrated rather than derived.
    """
    stats = np.empty(n_trials)
    for i in range(n_trials):
        x = (
            rng.standard_normal((NY, NX)) + 1j * rng.standard_normal((NY, NX))
        ).astype(np.complex64) / math.sqrt(2.0)
        y = np.fft.fft(x, axis=0).astype(np.complex64)
        det.reset()
        (res,) = det.push(y)
        stats[i] = res[4]
    return float(np.quantile(stats, 1.0 - PFA_SYS))


def _run_sweep(
    snr_grid: NDArray[np.float64], n_trials: int, seed: int = 0
) -> dict:
    """Sweep per-sample SNR; return MC Pd, localization rate, ROC stats."""
    rng = np.random.default_rng(seed)
    code = make_code()
    det = make_detector(build_ref(code))

    theta = _calibrate_theta(det, rng, n_trials=1000)
    bins = doppler_bins()

    pd = np.empty(len(snr_grid))
    loc = np.empty(len(snr_grid))  # fraction of detections at the true cell
    sig_stats_op = None  # stored stats at the operating SNR for the ROC
    op_idx = int(np.argmin(np.abs(snr_grid - OP_SNR)))  # transition-knee SNR

    for k, snr_db in enumerate(snr_grid):
        n_det = 0
        n_loc = 0
        op_store = np.empty(n_trials) if k == op_idx else None
        for i in range(n_trials):
            u = int(bins[rng.integers(len(bins))])
            cp = int(rng.integers(NX))
            y, dbin, col = build_acq_frame(
                rng,
                chips01=code,
                code_phase=cp,
                carrier_f=carrier_for_bin(u),
                snr_db=float(snr_db),
            )
            det.reset()
            (res,) = det.push(y)
            row, pcol, _, _, stat = res
            if op_store is not None:
                op_store[i] = stat
            if stat > theta:
                n_det += 1
                d_ok = min((row - dbin) % NY, (dbin - row) % NY) <= 1
                c_ok = abs(pcol - col) <= 1
                if d_ok and c_ok:
                    n_loc += 1
        pd[k] = n_det / n_trials
        loc[k] = (n_loc / n_det) if n_det else math.nan
        if op_store is not None:
            sig_stats_op = op_store

    # Noise-only stats for the ROC and the empirical Pfa.
    noise_stats = np.empty(n_trials)
    for i in range(n_trials):
        x = (
            rng.standard_normal((NY, NX)) + 1j * rng.standard_normal((NY, NX))
        ).astype(np.complex64) / math.sqrt(2.0)
        y = np.fft.fft(x, axis=0).astype(np.complex64)
        det.reset()
        (res,) = det.push(y)
        noise_stats[i] = res[4]

    return {
        "theta": theta,
        "pd": pd,
        "loc": loc,
        "op_idx": op_idx,
        "sig_stats_op": sig_stats_op,
        "noise_stats": noise_stats,
    }


def main() -> None:
    """Run the Monte-Carlo sweep and render the three-panel figure."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    rng = np.random.default_rng(0)
    code = make_code()

    # ── Panel 1 data: one high-SNR acquisition surface ──────────────────────
    u_true = 2
    cp_true = 37
    y, dbin, col = build_acq_frame(
        rng,
        chips01=code,
        code_phase=cp_true,
        carrier_f=carrier_for_bin(u_true),
        snr_db=-20.0,  # ~ +13 dB after the ~33 dB coherent gain
    )
    with Corr2D(build_ref(code), dwell=1) as c:
        surf = np.abs(c.execute(y)).reshape(NY, NX)
    peak_row, peak_col = np.unravel_index(surf.argmax(), (NY, NX))

    # ── Panels 2 & 3 data: the SNR sweep ────────────────────────────────────
    snr_grid = np.linspace(-42.0, -15.0, 13)
    n_trials = 1500
    print(f"Running {len(snr_grid)} SNR points x {n_trials} trials each…")
    sweep = _run_sweep(snr_grid, n_trials=n_trials, seed=1)
    theta = sweep["theta"]
    pd = sweep["pd"]
    op_idx = sweep["op_idx"]
    op_snr = float(snr_grid[op_idx])
    noise_stats = sweep["noise_stats"]
    sig_stats_op = sweep["sig_stats_op"]
    pfa_mc = float((noise_stats > theta).mean())
    print(f"Calibrated gate theta = {theta:.3f}  (target Pfa = {PFA_SYS:.0e})")
    print(f"Empirical Pfa = {pfa_mc:.4f}")

    # Marcum-Q theory guide: per-sample amplitude SNR amplified by the coherent
    # gain sqrt(N); Bonferroni per-cell threshold for the N-cell search.
    pfa_cell = 1.0 - (1.0 - PFA_SYS) ** (1.0 / N)
    eta = det_threshold(pfa_cell)
    snr_eff = np.sqrt(N) * 10.0 ** (snr_grid / 20.0)
    pd_theory = np.array([det_pd(float(s), 1, eta) for s in snr_eff])

    fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(14, 4.5))
    fig.suptitle(
        f"DSSS burst acquisition — Detector2D  "
        f"({NY} Doppler x {NX} code-phase, SF={SF}, SPS={SPS}, "
        f"N={N} cells)",
        fontsize=10,
    )

    # Panel 1 — acquisition surface
    im = ax1.imshow(surf, origin="upper", cmap="viridis", aspect="auto")
    ax1.plot(col, dbin, "w+", ms=14, mew=2, label="true cell")
    ax1.plot(
        peak_col,
        peak_row,
        "ro",
        ms=9,
        mfc="none",
        mew=1.8,
        label=f"peak (D={peak_row}, CP={peak_col})",
    )
    ax1.set_xlabel("code-phase bin (samples)")
    ax1.set_ylabel("Doppler bin")
    ax1.set_title("Acquisition surface $|R|$")
    ax1.legend(fontsize=8, loc="upper right")
    fig.colorbar(im, ax=ax1, fraction=0.046)

    # Panel 2 — Pd vs SNR
    ax2.plot(snr_grid, pd, "o-", color="C1", label=f"MC ({n_trials} trials)")
    ax2.plot(snr_grid, pd_theory, color="C0", lw=2, label="Marcum-Q guide")
    ax2.axvline(op_snr, color="0.7", ls=":", lw=1)
    ax2.text(
        op_snr + 0.3, 0.05, f"op {op_snr:.0f} dB", color="0.4", fontsize=8
    )
    ax2.set_ylim(-0.02, 1.05)
    ax2.set_xlabel("per-sample SNR (dB)")
    ax2.set_ylabel(r"$P_d$")
    ax2.set_title(r"$P_d$ vs SNR")
    ax2.legend(fontsize=9)
    ax2.grid(True, ls=":", lw=0.6, alpha=0.8)

    # Panel 3 — ROC at the operating SNR
    thr = np.percentile(
        np.concatenate([sig_stats_op, noise_stats]), np.linspace(0, 100, 400)
    )
    roc_pfa = np.array([(noise_stats > t).mean() for t in thr])
    roc_pd = np.array([(sig_stats_op > t).mean() for t in thr])
    ax3.plot(roc_pfa, roc_pd, color="C0", lw=1.5, label="MC (swept threshold)")
    ax3.plot(
        pfa_mc,
        pd[op_idx],
        "o",
        color="C1",
        ms=8,
        zorder=5,
        label=f"operating point ($P_d$={pd[op_idx]:.3f})",
    )
    ax3.axvline(PFA_SYS, color="0.6", ls=":", lw=1, label=r"target $P_{fa}$")
    ax3.set_xscale("log")
    ax3.set_xlim(1e-3, 1.0)
    ax3.set_ylim(-0.02, 1.05)
    ax3.set_xlabel(r"$P_{fa}$")
    ax3.set_ylabel(r"$P_d$")
    ax3.set_title(f"ROC at {op_snr:.0f} dB")
    ax3.legend(fontsize=8, loc="lower right")
    ax3.grid(True, which="both", ls=":", lw=0.6, alpha=0.8)

    fig.tight_layout()
    out = "detector2d_acq_demo.png"
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"saved {out}")


if __name__ == "__main__":
    main()
