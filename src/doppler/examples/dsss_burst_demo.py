"""dsss_burst_demo.py — DSSS acquisition preamble + BPSK payload + silence.

A parameterisable DSSS burst built from the doppler wfm Composer::

    [ acq_reps × PN period | BPSK payload | silence ]

The acquisition code is a Galois LFSR maximum-length sequence (MLS) of length
``2^L − 1`` chips.  Repeating it ``acq_reps`` times gives the receiver enough
coherent integration to detect the signal at low SNR — more repetitions lowers
the detection threshold by ``10·log10(acq_reps)`` dB.  The payload runs at the
same chip rate (``sps × L`` samples per BPSK symbol = spreading factor SF =
period chips per data bit), so the occupied bandwidth is identical throughout:
a DSSS burst looks like noise from start to silence.

Four panels
-----------
Top-left
    Spectrogram — the three burst phases in time × frequency (acquisition,
    payload, silence).  The flat noise-like floor is the DSSS spread spectrum.

Top-right
    Sliding cross-correlation with one PN period — magnitude of the received
    signal correlated against the clean reference code at chip-rate spacing.
    Sharp peaks every ``PERIOD`` chips mark the acquisition code boundaries;
    the payload interval shows a lower, uncorrelated floor.

Bottom-left
    Spectrum — acquisition PSD vs payload PSD overlaid.  Both are flat across
    the chip bandwidth (the MLS has a near-ideal flat power spectrum), so the
    two phases are spectrally indistinguishable.

Bottom-right
    Detection SNR vs input SNR for three ``acq_reps`` values.  Each additional
    repetition is one coherent averaging step; the peak cross-correlation SNR
    rises by ``10·log10(acq_reps)`` dB, trading burst duration for sensitivity.

Run
---
::

    python examples/python/dsss_burst_demo.py
"""

from __future__ import annotations

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from doppler.spectral import PSD, Corr
from doppler.wfm import Composer, Segment, Synth

# ── burst geometry ───────────────────────────────────────────────────────────
PN_LENGTH = 7  # LFSR register length; MLS period = 2^L − 1 = 127 chips
PERIOD = 2**PN_LENGTH - 1
CHIP_SPS = 2  # samples per chip; chip rate = FS / CHIP_SPS = 500 kHz
FS = 1e6  # baseband sample rate (Hz)


# ── helpers ──────────────────────────────────────────────────────────────────


def pn_reference() -> np.ndarray:
    """One clean PN period as a real ±1 float array (length PERIOD × CHIP_SPS).

    The default SNR of 100 dB skips AWGN entirely so chips are unambiguously
    ±1.  The imaginary part is negligible for a baseband PN source and is
    discarded.  The same code seed (default 1) is used throughout, matching
    the transmitter.
    """
    return (
        Synth(type="pn", fs=FS, pn_length=PN_LENGTH, sps=CHIP_SPS)
        .steps(PERIOD * CHIP_SPS)
        .real
    )


def build_burst(
    acq_reps: int = 4,
    n_payload_sym: int = 48,
    snr_db: float = 20.0,
    silence_chips: int = 256,
    seed: int = 0,
) -> np.ndarray:
    """Build one DSSS burst: acquisition preamble → BPSK payload → silence.

    The Composer resolves a shared AWGN noise floor from the ``snr_db``
    parameter so both the acquisition and payload segments sit at the same
    over-sample-rate SNR.

    Parameters
    ----------
    acq_reps : int, default 4
        Number of PN code periods in the acquisition preamble.  Each extra
        repetition buys ``10·log10(acq_reps / (acq_reps − 1))`` dB of
        coherent detection gain at the receiver's correlator.
    n_payload_sym : int, default 48
        BPSK symbol count in the payload.  Each symbol spans one complete PN
        period (spreading factor SF = ``PERIOD`` = 127 chips), so the payload
        occupies ``n_payload_sym × PERIOD × CHIP_SPS`` samples and its
        bandwidth equals the chip bandwidth.
    snr_db : float, default 20.0
        Signal-to-noise ratio (dB, over the full sample-rate band) applied to
        both the acquisition and payload phases.  The Composer resolves
        this into the AWGN floor; both phases share one noise realisation.
    silence_chips : int, default 256
        Trailing zero-gap after the payload, in chips (``× CHIP_SPS`` samples).
        Models dead time between bursts.
    seed : int, default 0
        BPSK payload seed.  Changing it selects a different random data
        sequence while keeping the PN code fixed.

    Returns
    -------
    np.ndarray
        Complex64 baseband IQ burst array.

    Examples
    --------
    >>> burst = build_burst(acq_reps=3, snr_db=15.0)
    >>> burst.dtype
    dtype('complex64')
    >>> # acquisition + payload + silence
    >>> expected = (3 + 48) * 127 * 2 + 256 * 2
    >>> len(burst) == expected
    True
    """
    acq_samples = acq_reps * PERIOD * CHIP_SPS
    payload_samples = n_payload_sym * PERIOD * CHIP_SPS
    off_samples = silence_chips * CHIP_SPS

    # Acquisition: acq_reps consecutive PN periods, same chip rate as payload.
    acq = Segment(
        type="pn",
        pn_length=PN_LENGTH,
        sps=CHIP_SPS,
        snr=snr_db,
        fs=FS,
        num_samples=acq_samples,
    )
    # Payload: BPSK data at one symbol per PN period (SF = PERIOD chips/bit).
    # sps = PERIOD * CHIP_SPS so each BPSK pulse occupies exactly one PN
    # period duration — the chip bandwidth is unchanged.
    payload = Segment(
        type="bpsk",
        sps=PERIOD * CHIP_SPS,
        snr=snr_db,
        fs=FS,
        seed=seed,
        num_samples=payload_samples,
        off_samples=off_samples,
    )
    return Composer([acq, payload]).compose()


def sliding_xcorr(x: np.ndarray, ref: np.ndarray) -> np.ndarray:
    """Magnitude of x cross-correlated with ref, sampled at chip-rate spacing.

    Uses ``doppler.spectral.Corr`` (FFT-based, O(n log n) per frame) to
    evaluate ``|R[0]|`` at each chip-aligned window.  Lag 0 is the matched
    lag since windows are stepped by exactly one chip (``CHIP_SPS`` samples).

    Parameters
    ----------
    x : np.ndarray
        Received baseband burst (complex64).
    ref : np.ndarray
        Clean PN reference, length ``PERIOD × CHIP_SPS`` (real float32).

    Returns
    -------
    np.ndarray
        Correlation magnitude; length ``(len(x) − len(ref)) // CHIP_SPS``.
    """
    indices = range(0, len(x) - len(ref) + 1, CHIP_SPS)
    with Corr(ref.astype(np.complex64), dwell=1) as c:
        return np.array([abs(c.execute(x[i : i + c.n])[0]) for i in indices])


def psd_db(x: np.ndarray, nfft: int = 512) -> np.ndarray:
    """Averaged Welch PSD in dB, normalised to its own peak."""
    est = PSD(n=nfft, fs=FS, window="hann", mode="mean")
    est.accumulate(x)
    p = est.psd_db()
    return p - p.max()


def spectrogram(x: np.ndarray, nfft: int = 128, hop: int = 32) -> np.ndarray:
    """Short-time FFT magnitude in dB, normalised to its peak."""
    win = np.hanning(nfft)
    cols = [
        np.fft.fftshift(np.abs(np.fft.fft(x[i : i + nfft] * win)))
        for i in range(0, len(x) - nfft, hop)
    ]
    s = 20.0 * np.log10(np.array(cols).T + 1e-9)
    return s - s.max()


# ── build reference and main burst ───────────────────────────────────────────
ACQ_REPS = 4
N_PAYLOAD_SYM = 48
SNR_DB = 18.0
SILENCE_CHIPS = 256

ref = pn_reference()
burst = build_burst(
    acq_reps=ACQ_REPS, n_payload_sym=N_PAYLOAD_SYM, snr_db=SNR_DB
)

# Annotate burst phase boundaries in samples
acq_end = ACQ_REPS * PERIOD * CHIP_SPS
payload_end = acq_end + N_PAYLOAD_SYM * PERIOD * CHIP_SPS

# ── cross-correlation with one PN period ─────────────────────────────────────
xcorr = sliding_xcorr(burst, ref)
xcorr_t = np.arange(len(xcorr)) * CHIP_SPS / FS * 1e3  # ms

# ── SNR sweep: detection peak vs input SNR for several acq_reps values ───────
snr_range = np.arange(-10, 31, 2, dtype=float)
reps_sweep = [1, 2, 4, 8]
detection_snr: dict[int, list[float]] = {r: [] for r in reps_sweep}

for snr in snr_range:
    for reps in reps_sweep:
        b = build_burst(
            acq_reps=reps, n_payload_sym=0, snr_db=snr, silence_chips=0
        )
        xc = sliding_xcorr(b, ref)
        # Peak in the acquisition window
        peak = float(xc.max())
        # Noise floor: std of the correlation outside the peaks
        mask = xc < 0.8 * peak
        noise_floor = float(xc[mask].std()) if mask.any() else 1.0
        detection_snr[reps].append(
            20.0 * np.log10(peak / (noise_floor + 1e-9))
        )

# ── plot ─────────────────────────────────────────────────────────────────────
fig, axes = plt.subplots(2, 2, figsize=(13, 9))
fig.suptitle(
    f"DSSS burst — {ACQ_REPS}× PN acq | {N_PAYLOAD_SYM}-sym BPSK payload"
    f"  (PN length {PN_LENGTH}, period {PERIOD} chips, {SNR_DB:.0f} dB SNR)",
    fontsize=12,
    fontweight="bold",
)

# ── A: spectrogram ───────────────────────────────────────────────────────────
ax = axes[0, 0]
sg = spectrogram(burst)
t_sg = np.linspace(0, len(burst) / FS * 1e3, sg.shape[1])
f_sg = np.linspace(-FS / 2e3, FS / 2e3, sg.shape[0])
ax.pcolormesh(t_sg, f_sg, sg, vmin=-50, vmax=0, cmap="magma", shading="auto")
# Phase boundary markers
for t_samp, label, ha in (
    (acq_end, "acq end", "right"),
    (payload_end, "payload end", "right"),
):
    t_ms = t_samp / FS * 1e3
    ax.axvline(t_ms, color="w", lw=1.0, ls="--", alpha=0.7)
    ax.text(
        t_ms - 0.1,
        FS / 2.5e3,
        label,
        color="w",
        fontsize=7,
        ha=ha,
        va="top",
    )
ax.set_xlabel("time (ms)")
ax.set_ylabel("frequency (kHz)")
ax.set_title("Spectrogram — acquisition | payload | silence")

# ── B: cross-correlation ─────────────────────────────────────────────────────
ax = axes[0, 1]
ax.plot(xcorr_t, xcorr, lw=0.7, color="#1f77b4")
# Mark acquisition region
ax.axvspan(
    0, acq_end / FS * 1e3, alpha=0.12, color="#2ca02c", label="acquisition"
)
ax.axvspan(
    acq_end / FS * 1e3,
    payload_end / FS * 1e3,
    alpha=0.12,
    color="#ff7f0e",
    label="payload",
)
ax.set_xlabel("time (ms)")
ax.set_ylabel("|x ⋆ PN| (linear)")
ax.set_title(f"Sliding PN cross-correlation — peaks every {PERIOD} chips")
ax.legend(fontsize=8, loc="upper right")
ax.grid(alpha=0.3)

# ── C: spectrum — acquisition vs payload ─────────────────────────────────────
ax = axes[1, 0]
f_ax = np.fft.fftshift(np.fft.fftfreq(512, 1 / FS)) / 1e3
ax.plot(
    f_ax, psd_db(burst[:acq_end]), lw=1.0, color="#2ca02c", label="acquisition"
)
ax.plot(
    f_ax,
    psd_db(burst[acq_end:payload_end]),
    lw=1.0,
    color="#ff7f0e",
    ls="--",
    label="payload",
)
ax.set_xlabel("frequency (kHz)")
ax.set_ylabel("PSD (dB, rel. peak)")
ax.set_title("Spectrum — acquisition vs payload (both flat DSSS)")
ax.set_ylim(-30, 5)
ax.legend(fontsize=9)
ax.grid(alpha=0.3)

# ── D: detection SNR vs input SNR (acq_reps sweep) ───────────────────────────
ax = axes[1, 1]
colors = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728"]
for reps, color in zip(reps_sweep, colors):
    ax.plot(
        snr_range,
        detection_snr[reps],
        lw=1.5,
        color=color,
        label=f"{reps} rep{'s' if reps > 1 else ''}",
    )
# Theoretical coherent gain lines
for reps, color in zip(reps_sweep, colors):
    theory = snr_range + 10 * np.log10(PERIOD * reps)
    ax.plot(snr_range, theory, lw=0.8, color=color, ls=":", alpha=0.6)
ax.set_xlabel("input SNR (dB)")
ax.set_ylabel("detection peak SNR (dB)")
ax.set_title(
    "Detection SNR vs input SNR — coherent gain of acq repetitions\n"
    "(solid = measured, dotted = theory +10·log10(PERIOD·reps))"
)
ax.legend(title="acq_reps", fontsize=8)
ax.grid(alpha=0.3)

fig.tight_layout(rect=(0, 0, 1, 0.95))
fig.savefig("dsss_burst_demo.png", dpi=110)

# ── summary ──────────────────────────────────────────────────────────────────
acq_ms = acq_end / FS * 1e3
payload_ms = (payload_end - acq_end) / FS * 1e3
silence_ms = (len(burst) - payload_end) / FS * 1e3
print(
    f"burst: {len(burst)} samples at {FS / 1e6:.0f} MHz\n"
    f"  acquisition : {acq_ms:.2f} ms  ({ACQ_REPS}× {PERIOD}-chip MLS)\n"
    f"  payload     : {payload_ms:.2f} ms  ({N_PAYLOAD_SYM} BPSK syms)\n"
    f"  silence     : {silence_ms:.2f} ms  ({SILENCE_CHIPS} chips)\n"
    f"  SNR         : {SNR_DB:.0f} dB  →  wrote dsss_burst_demo.png"
)
