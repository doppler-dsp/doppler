"""dsss_acq_characterization.py — Pd / Pfa vs Es/N0 for DSSS acquisition.

Scenario
--------
A direct-sequence spread-spectrum transmitter emits a burst:

    [ silence | 5x length-511 PN preamble | BPSK DSSS payload | silence ]

The acquisition preamble is a 9-stage Galois maximal-length sequence (MLS),
period ``2**9 - 1 = 511`` chips, repeated ``ACQ_REPS = 5`` times so the
receiver can integrate it coherently.  The payload is random BPSK data spread
at ``DATA_SF = 64`` chips per symbol over a *distinct* code.  The whole burst
reaches the receiver at a **random integer code phase** and a **random carrier
(Doppler) offset drawn uniformly across the engine's capture range**
(``+/- chip_rate / (2*sf)``), buried in AWGN.

``doppler.dsss.Acquisition`` must acquire the preamble: frame the stream, run a
slow-time Doppler FFT, correlate against the PN reference, and gate the peak on
an automatically configured CFAR threshold.  We sweep the noise floor and
measure, as a function of the data-link **Es/N0**:

* ``Pd`` — fraction of trials whose detection lands on the true
  ``(Doppler bin, code phase)`` cell, averaged over random code phase and
  random Doppler across the capture range.  The average folds in the Doppler
  scalloping and within-segment rotation losses of the coarse 5-bin search, so
  the knee sits a few dB above the raw ``10*log10(sf*reps)`` coherent gain.
* ``Pfa`` — false-alarm rate measured on the noise-only (silence) frames.  It
  is set by the CFAR threshold and is independent of the signal, so it tracks
  the configured target across the sweep.

Use of the wfm "wfmgen" surface
-------------------------------
Everything radiated is generated through ``doppler.wfm``: the PN preamble comes
from ``Synth(type="pn")`` and the spread payload from ``dsss_spread``.  Only
the *channel* — integer code-phase delay, Doppler carrier, leading/trailing
silence, and the AWGN floor — is applied around the wfmgen output.

Es/N0
-----
With a unit-power chip (``|chip| = 1``) and complex AWGN of per-sample variance
``sigma**2`` (so the per-sample power SNR is ``gamma = 1 / sigma**2``):

* energy per chip   ``Ec      = SPC / fs = 1 / chip_rate``
* noise density     ``N0      = sigma**2 / fs``
* per chip          ``Ec/N0   = SPC * gamma``           (``+10log10(SPC)`` dB)
* per data symbol   ``Es/N0   = DATA_SF * Ec/N0``       (``DATA_SF`` chips/sym)

so in dB ``Es/N0 = snr_db_per_sample + 10*log10(DATA_SF * SPC)``.  With
``SPC = 1`` that is a flat ``+10*log10(64) = +18.06`` dB offset from the
per-sample SNR the acquisition engine sizes against.

Panels (saved to ``dsss_acq_characterization.png``)
---------------------------------------------------
1. One received burst's power envelope vs time — silence / preamble / payload /
   silence — with the frame the engine fires on marked.
2. ``Pd`` vs Es/N0 — Monte-Carlo S-curve over random code phase + Doppler, with
   the configured ``pd`` target marked.
3. ``Pfa`` vs Es/N0 — per-point silence-frame estimate plus the achieved rate
   over a large noise-only run, against the configured ``pfa`` target.

Run::

    python -m doppler.examples.dsss_acq_characterization

Runs in ~30 s.
"""

import math
import warnings

import numpy as np
from numpy.typing import NDArray

from doppler.dsss import Acquisition
from doppler.wfm import PN, Synth, dsss_spread, mls_poly

# ── burst geometry ───────────────────────────────────────────────────────────
PN_LENGTH = 9  # LFSR stages; MLS period SF = 2**9 - 1 = 511 chips
SF = 2**PN_LENGTH - 1  # 511-chip acquisition code (the "length-512" family)
ACQ_REPS = 5  # coherent preamble repetitions
SPC = 1  # samples per chip (chip-rate sampled)
CHIP_RATE = 1.024e6  # Hz
FS = CHIP_RATE * SPC  # baseband sample rate
DATA_SF = 64  # data chips per payload symbol (sets Es/N0)
N_DATA_SYM = 40  # payload data symbols
K_PRE = 3  # leading silence, in whole acquisition frames
K_POST = 3  # trailing silence, in whole acquisition frames

# ── detection targets ────────────────────────────────────────────────────────
PFA = 1e-3  # configured system false-alarm probability
PD = 0.9  # configured target detection probability
# Conservative sizing C/N0: low enough that the engine exhausts its coherent
# depth to ACQ_REPS (doppler_bins == reps), pinning the frame to the preamble.
CN0_SIZE = 26.0  # dB-Hz

# The acquisition reference: one 511-chip MLS period (0/1 chips).  This is the
# exact code radiated by ``Synth(type="pn", pn_length=9)`` — verified identical
# to ``PN(poly=mls_poly(9), seed=1).generate(511)`` mapped chip 1 -> -1.
ACQ_CODE: NDArray[np.uint8] = (
    PN(poly=mls_poly(PN_LENGTH), seed=1, length=PN_LENGTH)
    .generate(SF)
    .astype(np.uint8)
)
# A distinct code for the payload so it decorrelates from the preamble.
DATA_CODE: NDArray[np.uint8] = (
    PN(poly=mls_poly(6), seed=3, length=6).generate(DATA_SF).astype(np.uint8)
)

# One clean (noise-free) 5-rep preamble in wfm "wfmgen" form: BPSK ±1 chips,
# no carrier.  Code phase (a circular roll) and Doppler (a carrier) are applied
# per trial; because the preamble is exactly ACQ_REPS whole periods, a
# roll < SF is a clean circular code delay.
_PREAMBLE0: NDArray[np.complex64] = Synth(
    type="pn", pn_length=PN_LENGTH, sps=SPC, snr=100.0, fs=FS
).steps(ACQ_REPS * SF * SPC)


def make_engine() -> Acquisition:
    """Build an acquisition engine pinned to ``doppler_bins == ACQ_REPS``.

    The conservative ``CN0_SIZE`` cannot meet ``PD`` on the reps-deep grid, so
    construction emits an "under-powered" ``UserWarning`` — expected here (the
    injected bursts are far stronger than the sizing point), and filtered.

    Returns
    -------
    Acquisition
        A fresh engine; ``code_bins == SF``, ``doppler_bins == ACQ_REPS``.

    Examples
    --------
    >>> a = make_engine()
    >>> a.code_bins, a.doppler_bins
    (511, 5)
    """
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        a = Acquisition(
            ACQ_CODE,
            reps=ACQ_REPS,
            spc=SPC,
            chip_rate=CHIP_RATE,
            cn0_dbhz=CN0_SIZE,
            doppler_uncertainty=0.0,  # search the full capture range
            pfa=PFA,
            pd=PD,
        )
    assert a.doppler_bins == ACQ_REPS, "sizing failed to pin doppler_bins"
    return a


# Frame size in samples — one engine, one number; reused everywhere.
_E0 = make_engine()
FRAME = _E0.code_bins * _E0.doppler_bins  # = SF * ACQ_REPS = 2555
DOPPLER_BINS = _E0.doppler_bins
DOPPLER_RES_HZ = _E0.doppler_res_hz
DOPPLER_SPAN_HZ = _E0.doppler_span_hz


def es_n0_db(snr_db_per_sample: float) -> float:
    """Data-symbol Es/N0 (dB) for a per-sample power SNR (dB).

    Examples
    --------
    >>> round(es_n0_db(-18.0), 2)  # +10log10(64) above the per-sample SNR
    0.06
    """
    return snr_db_per_sample + 10.0 * math.log10(DATA_SF * SPC)


def snr_db_for_es_n0(es_n0: float) -> float:
    """Per-sample power SNR (dB) for a target data-symbol Es/N0 (dB)."""
    return es_n0 - 10.0 * math.log10(DATA_SF * SPC)


def build_scene(
    snr_db: float,
    code_phase: int,
    doppler_hz: float,
    seed: int,
) -> tuple[NDArray[np.complex64], list[str]]:
    """Build one received burst and its per-frame kind labels.

    The clean preamble and payload are wfmgen output; the channel (code-phase
    roll, Doppler carrier, frame-aligned silence, AWGN) is applied here.

    Parameters
    ----------
    snr_db : float
        Per-sample power SNR (dB).  Chip power is 1, so the complex-AWGN scale
        is ``sigma = 10**(-snr_db / 20)``.
    code_phase : int
        Integer-sample code delay in ``[0, SF)`` — the column the engine should
        report.
    doppler_hz : float
        Carrier offset (Hz); should lie within ``+/- DOPPLER_SPAN_HZ``.
    seed : int
        Seeds the payload bits and the noise realisation.

    Returns
    -------
    samples : NDArray[np.complex64]
        The full received stream, an integer number of acquisition frames.
    kinds : list[str]
        One label per frame: ``"sil"`` (noise only), ``"pre"`` (preamble), or
        ``"pay"`` (payload).

    Examples
    --------
    >>> x, kinds = build_scene(0.0, code_phase=137, doppler_hz=400.0, seed=0)
    >>> len(x) % FRAME, kinds.count("pre")
    (0, 1)
    """
    rng = np.random.default_rng(1000 + seed)

    # Preamble: circular code-phase roll, then the Doppler carrier.
    pre = np.roll(_PREAMBLE0, code_phase * SPC).astype(np.complex64)
    pre *= np.exp(2j * np.pi * (doppler_hz / FS) * np.arange(len(pre))).astype(
        np.complex64
    )

    # Payload: random BPSK data spread at DATA_SF chips/symbol (distinct code),
    # carried at the same Doppler; padded to a whole number of frames.
    bits = rng.integers(0, 2, N_DATA_SYM).astype(np.uint8)
    syms = np.where(bits & 1, -1.0, 1.0).astype(np.complex64)
    pay = np.repeat(dsss_spread(syms, DATA_CODE, DATA_SF), SPC).astype(
        np.complex64
    )
    pay *= np.exp(2j * np.pi * (doppler_hz / FS) * np.arange(len(pay))).astype(
        np.complex64
    )
    n_pay_frames = (len(pay) + FRAME - 1) // FRAME
    pay = np.concatenate(
        [pay, np.zeros(n_pay_frames * FRAME - len(pay), np.complex64)]
    )

    sig = np.concatenate(
        [
            np.zeros(K_PRE * FRAME, np.complex64),
            pre,
            pay,
            np.zeros(K_POST * FRAME, np.complex64),
        ]
    )
    sigma = 10.0 ** (-snr_db / 20.0)
    noise = (sigma / math.sqrt(2.0)) * (
        rng.standard_normal(len(sig)) + 1j * rng.standard_normal(len(sig))
    )
    samples = (sig + noise).astype(np.complex64)
    kinds = (
        ["sil"] * K_PRE + ["pre"] + ["pay"] * n_pay_frames + ["sil"] * K_POST
    )
    return samples, kinds


def _true_cell(code_phase: int, doppler_hz: float) -> tuple[int, int]:
    """The (Doppler bin, code column) the engine should report for a trial."""
    row = round(doppler_hz / DOPPLER_RES_HZ) % DOPPLER_BINS
    return row, code_phase


def run_trial(snr_db: float, seed: int) -> tuple[bool, int, int]:
    """Acquire one random-(code phase, Doppler) burst, frame by frame.

    Pushing one frame at a time (the engine frames on a fixed grid anchored at
    sample 0) attributes every emitted hit to the frame that produced it, so a
    detection on the preamble frame and a false alarm on a silence frame are
    cleanly separated.

    Returns
    -------
    detected : bool
        A hit landed on the true cell (Doppler bin exact, code column +/-1) in
        the preamble frame.
    n_false_silence : int
        Hits emitted on the noise-only (silence) frames — the Pfa numerator.
    n_silence_frames : int
        Silence frames pushed — the Pfa denominator for this trial.
    """
    rng = np.random.default_rng(7000 + seed)
    code_phase = int(rng.integers(0, SF))
    doppler_hz = float(rng.uniform(-DOPPLER_SPAN_HZ, DOPPLER_SPAN_HZ))
    samples, kinds = build_scene(snr_db, code_phase, doppler_hz, seed)
    row_t, col_t = _true_cell(code_phase, doppler_hz)

    a = make_engine()
    detected = False
    n_false = 0
    n_sil = 0
    for g, kind in enumerate(kinds):
        hits = a.push(samples[g * FRAME : (g + 1) * FRAME])
        if kind == "sil":
            n_sil += 1
        for dop, col, *_ in hits:
            on_cell = (
                min((dop - row_t) % DOPPLER_BINS, (row_t - dop) % DOPPLER_BINS)
                == 0
                and min((col - col_t) % SF, (col_t - col) % SF) <= 1
            )
            if kind == "pre" and on_cell:
                detected = True
            elif kind == "sil":
                n_false += 1
    return detected, n_false, n_sil


def measure_pd(
    snr_grid: NDArray[np.float64], n_trials: int, seed: int = 0
) -> tuple[NDArray[np.float64], NDArray[np.float64]]:
    """Monte-Carlo Pd and per-point silence Pfa over the SNR grid.

    Returns
    -------
    pd : NDArray[np.float64]
        Detection probability at each grid point.
    pfa_silence : NDArray[np.float64]
        False-alarm rate over that point's silence frames (noisy; the dedicated
        ``measure_pfa`` run is the precise estimate).
    """
    pd = np.empty(len(snr_grid))
    pfa = np.empty(len(snr_grid))
    for k, snr_db in enumerate(snr_grid):
        n_det = n_false = n_sil = 0
        for t in range(n_trials):
            det, false, sil = run_trial(float(snr_db), seed + k * n_trials + t)
            n_det += det
            n_false += false
            n_sil += sil
        pd[k] = n_det / n_trials
        pfa[k] = n_false / max(n_sil, 1)
    return pd, pfa


def measure_pfa(n_frames: int, seed: int = 0) -> float:
    """Achieved system Pfa over a pure-noise run (CFAR → signal-independent).

    Pushes ``n_frames`` unit-power complex-noise frames and counts hits.
    The CFAR test statistic is scale-invariant, so the rate does not depend on
    the noise level — one number characterises the threshold.
    """
    rng = np.random.default_rng(20240 + seed)
    a = make_engine()
    hits = 0
    pushed = 0
    chunk = 64  # frames per push
    while pushed < n_frames:
        f = min(chunk, n_frames - pushed)
        m = f * FRAME
        x = (1.0 / math.sqrt(2.0)) * (
            rng.standard_normal(m) + 1j * rng.standard_normal(m)
        )
        hits += len(a.push(x.astype(np.complex64)))
        pushed += f
    return hits / n_frames


# ── plotting ─────────────────────────────────────────────────────────────────


def acq_surface(frame: NDArray[np.complex64]) -> NDArray[np.float64]:
    """The engine's |R| decision surface for one acquisition frame.

    Reproduces, with NumPy, exactly what ``Acquisition`` computes internally
    per frame — reframe to ``(ACQ_REPS, SF)`` (one row per code repetition),
    take the **slow-time Doppler FFT** down the rows (the coherent stack of all
    ``ACQ_REPS`` repetitions), then **circularly correlate** each row against
    the PN code along the fast-time axis.  The ``argmax`` of the returned
    ``(ACQ_REPS, SF)`` surface is the ``(doppler_bin, code_phase)`` cell the
    engine reports — verified bit-for-bit against the engine's emitted hit.

    Parameters
    ----------
    frame : NDArray[np.complex64]
        Exactly ``FRAME`` samples (one acquisition frame).

    Returns
    -------
    NDArray[np.float64]
        ``(ACQ_REPS, SF)`` magnitude surface: Doppler bin x code phase.
    """
    f2 = np.asarray(frame, np.complex64).reshape(ACQ_REPS, SF)
    doppler = np.fft.fft(f2, axis=0)  # slow-time stack of the reps
    ref = np.where(ACQ_CODE & 1, -1.0, 1.0).astype(np.complex64)
    code_corr = np.fft.ifft(
        np.fft.fft(doppler, axis=1) * np.conj(np.fft.fft(ref))[None, :],
        axis=1,
    )
    return np.abs(code_corr)


def main(out_path: str = "dsss_acq_characterization.png") -> None:
    """Render the three-panel characterisation to ``out_path``."""
    import matplotlib

    matplotlib.use("Agg")  # headless: render straight to a file
    import matplotlib.pyplot as plt

    # Grid in data-symbol Es/N0 (dB), mapped back to the per-sample SNR sweep.
    es_n0_grid = np.array([-6.0, -3.0, 0.0, 2.0, 4.0, 6.0, 9.0])
    snr_grid = np.array([snr_db_for_es_n0(e) for e in es_n0_grid])
    n_trials = 200

    print(
        f"DSSS acq: SF={SF}, reps={ACQ_REPS}, "
        f"chip_rate={CHIP_RATE / 1e6:g} MHz, "
        f"capture +/-{DOPPLER_SPAN_HZ:.0f} Hz, frame={FRAME} samples"
    )
    print(f"sweeping {len(es_n0_grid)} Es/N0 points x {n_trials} trials…")
    pd, pfa_sil = measure_pd(snr_grid, n_trials=n_trials, seed=1)
    n_noise = 20000
    pfa_hat = measure_pfa(n_noise, seed=5)
    print(f"achieved Pfa over {n_noise} noise frames: {pfa_hat:.2e}")

    # A representative burst for panel 1: a known cell, strong enough for a
    # crisp surface.  Run the engine to get the ACTUAL emitted detection, and
    # reconstruct the same frame's decision surface to plot it on.
    demo_cp, demo_dop = 211, 350.0
    x_demo, kinds = build_scene(
        snr_db_for_es_n0(12.0), code_phase=demo_cp, doppler_hz=demo_dop, seed=3
    )
    true_row, true_col = _true_cell(demo_cp, demo_dop)
    pre_g = kinds.index("pre")
    surface = acq_surface(x_demo[pre_g * FRAME : (pre_g + 1) * FRAME])
    det_hits = make_engine().push(x_demo)
    det_row, det_col = (int(det_hits[0][0]), int(det_hits[0][1]))

    fig, axes = plt.subplots(1, 3, figsize=(16, 4.6))
    fig.suptitle(
        f"DSSS acquisition — {ACQ_REPS}x length-{SF} PN preamble, "
        f"{DATA_SF} chips/symbol payload, {CHIP_RATE / 1e6:g} MHz chips",
        fontsize=12,
        fontweight="bold",
    )

    # Panel 1 — the engine's acquisition surface with the actual detection.
    ax = axes[0]
    im = ax.imshow(
        surface,
        aspect="auto",
        origin="lower",
        cmap="viridis",
        extent=(0, SF, -0.5, DOPPLER_BINS - 0.5),
        interpolation="nearest",
    )
    fig.colorbar(im, ax=ax, label="|R| (coherent)", fraction=0.046, pad=0.02)
    ax.plot(
        det_col,
        det_row,
        "o",
        ms=14,
        mfc="none",
        mec="red",
        mew=1.8,
        label=f"detection: bin {det_row}, φ {det_col}",
    )
    ax.plot(
        true_col,
        true_row,
        "+",
        ms=12,
        color="white",
        mew=1.8,
        label=f"injected: bin {true_row}, φ {true_col}",
    )
    ax.set_yticks(range(DOPPLER_BINS))
    ax.set_xlabel("code phase (chips)")
    ax.set_ylabel("Doppler bin")
    ax.set_title(
        "Acquisition surface — the engine's actual detection\n"
        f"(reframe → Doppler FFT stacks {ACQ_REPS} reps → code correlation)"
    )
    ax.legend(fontsize=8, loc="upper right")

    # Panel 2 — Pd vs Es/N0.
    ax = axes[1]
    ax.plot(es_n0_grid, pd, "o-", color="C1", label=f"MC ({n_trials} trials)")
    ax.axhline(PD, color="0.6", ls="--", lw=1, label=f"target Pd = {PD}")
    ax.set_xlabel("data-symbol Es/N0 (dB)")
    ax.set_ylabel("Pd")
    ax.set_title(
        "Detection probability\n(avg over random code phase + Doppler)"
    )
    ax.set_ylim(-0.03, 1.03)
    ax.legend(fontsize=8, loc="lower right")
    ax.grid(alpha=0.3)

    # Panel 3 — Pfa vs Es/N0 (signal-independent CFAR rate).
    ax = axes[2]
    ax.semilogy(
        es_n0_grid,
        np.maximum(pfa_sil, 1e-6),
        "s",
        color="C3",
        ms=5,
        label="per-point silence frames",
    )
    ax.axhline(
        pfa_hat,
        color="C0",
        lw=1.5,
        label=f"achieved ({n_noise} noise frames)",
    )
    ax.axhline(PFA, color="0.6", ls="--", lw=1, label=f"target Pfa = {PFA:g}")
    ax.set_xlabel("data-symbol Es/N0 (dB)")
    ax.set_ylabel("Pfa (per frame)")
    ax.set_title("False-alarm rate\n(CFAR: flat, signal-independent)")
    ax.set_ylim(1e-5, 1e-1)
    ax.legend(fontsize=8, loc="upper right")
    ax.grid(alpha=0.3, which="both")

    fig.tight_layout(rect=(0, 0, 1, 0.93))
    fig.savefig(out_path, dpi=110)
    print(f"Saved → {out_path}")


if __name__ == "__main__":
    main()
