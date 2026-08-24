"""dsss_burst_demo.py — DSSS acquisition preamble + BPSK payload + silence.

A parameterisable DSSS burst, described to wfmgen as one segment::

    [ acq_reps × acq_code | payload bits × data_code | silence ]

The acquisition code is a Galois LFSR maximum-length sequence (MLS) of length
``2^L − 1`` chips.  Repeating it ``acq_reps`` times gives the receiver enough
coherent integration to detect the signal at low SNR — more repetitions lowers
the detection threshold by roughly ``10·log10(acq_reps)`` dB — panel D
measures how much of that a real acquirer keeps.  The payload is spread by
a *second*, independent MLS (``DATA_SF`` = 31 chips per data bit) at the **same
chip rate**, so the occupied bandwidth is identical throughout: a DSSS burst
looks like noise from start to silence — a claim panel C plots and an
assertion below actually measures.

Both phases come from ONE declarative ``Segment(type="dsss")`` — wfmgen owns
the layout, the spreading and the segment's own length; this file names the
codes and reads the result.

Four panels
-----------
Top-left
    Spectrogram — the three burst phases in time × frequency (acquisition,
    payload, silence).  The flat noise-like floor is the DSSS spread spectrum.

Top-right
    Sliding cross-correlation with one preamble period — magnitude of the
    received signal correlated against the clean reference code at chip-rate
    spacing.  Sharp peaks every ``PERIOD`` chips mark the acquisition code
    boundaries; the payload rides a *different* code, so that interval shows
    only an uncorrelated floor.

Bottom-left
    Spectrum — acquisition PSD vs payload PSD overlaid.  Both are flat across
    the chip bandwidth (both are MLS-spread at the same chip rate), so the two
    phases are spectrally indistinguishable.

Bottom-right
    Detection probability vs input SNR, one curve per ``acq_reps``, measured
    by Monte Carlo through :class:`~doppler.dsss.BurstAcquisition` — the
    object that actually integrates coherently across the repetitions and
    applies the CFAR threshold.  Each doubling of ``acq_reps`` moves the
    Pd = 0.5 threshold about 2.5 dB left against an ideal
    ``10·log10(2)`` = 3.0 dB; the shortfall is the price of the larger search
    the extra coherent depth buys.  Measuring this with a hand-rolled
    one-period correlator is what doppler#980 was: it showed +0.6 dB across
    an 8x change and the assertion could not see it.

Run
---
::

    python dsss_burst_demo.py
"""

from __future__ import annotations

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from doppler.dsss import BurstAcquisition
from doppler.spectral import FFT, PSD, Corr, hann_window, magnitude_db_cf32
from doppler.wfm import PN, Composer, Segment, bpsk_map

# ── burst geometry ───────────────────────────────────────────────────────────
PN_LENGTH = 7  # preamble LFSR length; MLS period = 2^L − 1 = 127 chips
PERIOD = 2**PN_LENGTH - 1
DATA_PN_LENGTH = 5  # payload spreading code; a SECOND, shorter MLS
DATA_SF = 2**DATA_PN_LENGTH - 1  # spreading factor = 31 chips per data bit
CHIP_SPS = 2  # samples per chip; chip rate = FS / CHIP_SPS = 500 kHz
FS = 1e6  # baseband sample rate (Hz)


def _mls(length: int, n: int) -> np.ndarray:
    """One period of a maximal-length sequence (0/1) from the PN source.

    ``poly=0`` auto-picks the primitive polynomial for the register length,
    so the sequence has the thumbtack autocorrelation acquisition needs.
    """
    return (
        np.asarray(PN(poly=0, seed=1, length=length).generate(n)) & 1
    ).astype(np.uint8)


# The two codes the burst is described by: a long preamble code the receiver
# correlates against, and a short, independent data code that spreads every
# payload bit. They are handed to wfmgen below, so there is one definition of
# each — no separate transmitter and reference copy to drift apart.
ACQ_CODE = _mls(PN_LENGTH, PERIOD)
DATA_CODE = _mls(DATA_PN_LENGTH, DATA_SF)


# ── helpers ──────────────────────────────────────────────────────────────────


def pn_reference() -> np.ndarray:
    """One clean preamble period as a real ±1 array (PERIOD × CHIP_SPS long).

    Built from ``ACQ_CODE`` — the same array the transmitter is given — with
    :func:`~doppler.wfm.bpsk_map`, the C kernel wfmgen maps chips with, held
    for ``CHIP_SPS`` samples each. So the reference is the transmitted code
    by construction rather than by a matching seed.
    """
    return np.repeat(bpsk_map(ACQ_CODE).real, CHIP_SPS).astype(float)


def build_burst(
    acq_reps: int = 4,
    n_payload_sym: int = 48,
    snr_db: float = 20.0,
    silence_chips: int = 256,
    seed: int = 0,
) -> np.ndarray:
    """Build one DSSS burst: acquisition preamble → spread payload → silence.

    The burst is *described*, not assembled: one ``Segment(type="dsss")``
    names the two codes, the preamble repeat count and the payload bits, and
    wfmgen lays out ``[ acq_code × acq_reps | payload × data_code ]``, spreads
    every payload bit with the data code, sizes the segment to exactly one
    burst and resolves one AWGN floor across it.  Nothing here concatenates
    chips.

    Parameters
    ----------
    acq_reps : int, default 4
        Number of preamble code periods.  Each extra repetition buys
        ``10·log10(acq_reps / (acq_reps − 1))`` dB of coherent detection gain
        at the receiver's correlator.
    n_payload_sym : int, default 48
        BPSK symbol count in the payload.  Each bit is spread by the whole
        ``DATA_SF``-chip data code, so the payload occupies
        ``n_payload_sym × DATA_SF × CHIP_SPS`` samples and runs at the *same
        chip rate* as the preamble — which is why the two phases are
        spectrally indistinguishable.  Zero gives a preamble-only burst.
    snr_db : float, default 20.0
        Signal-to-noise ratio (dB, over the full sample-rate band) applied
        across the burst.  ``snr_mode="fs"`` pins that reading, so the sweep
        below plots detection SNR against a full-band input SNR.
    silence_chips : int, default 256
        Trailing zero-gap after the payload, in chips (``× CHIP_SPS`` samples).
        Models dead time between bursts.
    seed : int, default 0
        Payload data seed.  Changing it selects a different random bit
        sequence while keeping both codes fixed.

    Returns
    -------
    np.ndarray
        Complex64 baseband IQ burst array.

    Examples
    --------
    >>> burst = build_burst(acq_reps=3, snr_db=15.0)
    >>> burst.dtype
    dtype('complex64')
    >>> # preamble + spread payload + silence, in samples
    >>> expected = (3 * 127 + 48 * 31) * 2 + 256 * 2
    >>> len(burst) == expected
    True
    """
    payload = np.random.default_rng(seed).integers(
        0, 2, n_payload_sym, dtype=np.uint8
    )
    burst = Segment(
        type="dsss",
        fs=FS,
        sps=CHIP_SPS,  # samples per CHIP, both phases
        snr=snr_db,
        snr_mode="fs",
        seed=seed,
        acq_code=bytes(ACQ_CODE.tolist()),
        acq_reps=acq_reps,
        data_code=bytes(DATA_CODE.tolist()),
        sync=b"",  # no frame sync word: this demo detects, it does not decode
        payload=bytes(payload.tolist()),
        crc="none",
        off_samples=silence_chips * CHIP_SPS,
    )
    return Composer([burst]).compose()


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
    """Short-time FFT magnitude in dB, normalised to its peak.

    Every stage is a doppler primitive rather than its numpy equivalent:
    :func:`~doppler.spectral.hann_window` for the taper,
    :class:`~doppler.spectral.FFT` for the transform, and
    :func:`~doppler.spectral.magnitude_db_cf32` for the dB magnitude. The
    library owns each of those exactly once, and this is the same window and
    the same transform :class:`~doppler.spectral.PSD` uses in panel C — so
    the two spectral panels cannot disagree about what a Hann window is.
    """
    win = np.empty(nfft, np.float32)
    hann_window(win)
    with FFT(n=nfft) as fft:
        cols = [
            np.fft.fftshift(
                magnitude_db_cf32(
                    fft.execute_cf32(
                        (x[i : i + nfft] * win).astype(np.complex64)
                    ),
                    1e-9,  # -180 dB floor: the guard against log10(0)
                    0.0,
                )
            )
            for i in range(0, len(x) - nfft, hop)
        ]
    sg = np.array(cols).T
    return sg - sg.max()


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
payload_end = acq_end + N_PAYLOAD_SYM * DATA_SF * CHIP_SPS

# ── cross-correlation with one PN period ─────────────────────────────────────
xcorr = sliding_xcorr(burst, ref)
xcorr_t = np.arange(len(xcorr)) * CHIP_SPS / FS * 1e3  # ms

# ── validate: PN peaks at every period boundary in the acq window ────────────
# The preamble repeats the code ACQ_REPS times from sample 0, so the
# sliding correlator must peak at chips {0, PERIOD, 2·PERIOD, …} and
# nowhere else in the acquisition window.
acq_chips = acq_end // CHIP_SPS
peaks = np.flatnonzero(xcorr[:acq_chips] > 0.6 * xcorr[:acq_chips].max())
print(f"acq correlation peaks at chips {peaks.tolist()}")
assert len(peaks) == ACQ_REPS and np.all(peaks % PERIOD == 0), (
    "expected one correlation peak per PN period boundary"
)
# The payload is data, not the repeated code, so its correlation floor
# sits well below the acquisition peaks.
pay_floor = float(np.median(xcorr[acq_chips : payload_end // CHIP_SPS]))
peak_ratio = float(xcorr[:acq_chips].max() / pay_floor)
print(f"acq peak / payload floor = {peak_ratio:.1f}")
assert peak_ratio > 4.0, "acq peaks not distinct from the payload floor"

# ── validate: both phases really do occupy the same band ─────────────────────
# Panel C's whole claim, measured rather than asserted in prose: the fraction
# of spectrum bins within 10 dB of the peak, preamble vs payload.
#
# The bar comes from what the measurement actually separates. Spread, the two
# phases read 0.69 and 0.53 (ratio 0.77) — not identical, because the preamble
# is one 127-chip sequence repeated while the payload is a 31-chip code under
# random data, but both fill the chip band. Send the payload UNSPREAD, as one
# long pulse per bit, and it reads 0.01 (ratio 0.014): a 50x margin either
# side of a 0.5 bar. That unspread form is what this file shipped before the
# burst became one declarative dsss segment, and it is what this line catches.
occ_acq = float(np.mean(psd_db(burst[:acq_end]) > -10.0))
occ_pay = float(np.mean(psd_db(burst[acq_end:payload_end]) > -10.0))
print(f"band occupancy: acq {occ_acq:.2f}, payload {occ_pay:.2f}")
assert occ_pay / occ_acq > 0.5, (
    "the payload does not fill the chip band — is it actually spread?"
)

# ── detection sweep: Pd vs input SNR, one curve per acq_reps ─────────────────
# THIS IS THE OBJECT'S JOB, not a hand correlator's. `BurstAcquisition` is
# what integrates coherently across the preamble repetitions and applies the
# CFAR threshold; a sliding one-period matched filter cannot, which is exactly
# what this panel used to get wrong (doppler#980). Correlating against ONE
# period no matter how many were transmitted measured +0.6 dB across an 8x
# change in acq_reps while the docstring claimed +9 -- the plot showed four
# curves lying on top of each other under four separated theory lines.
#
# `configure_search_raw(reps, 1)` pins the search rather than letting it
# auto-size: `doppler_bins = reps` spends every repetition on the coherent
# axis (the gain being demonstrated), and `n_noncoh = 1` makes one burst
# enough to decide. The auto-sizer would pick n_noncoh > 1 for the small-reps
# arms, which need several frames this single-burst push can never supply --
# the same trap dsss_realtime_file_demod.py documents at its own push().
REPS_SWEEP = [1, 2, 4, 8]
SWEEP_SNRS = np.arange(-26.0, -7.0, 1.0)
SWEEP_TRIALS = 40  # independent noise draws per (reps, SNR) cell
DESIGN_CN0_DBHZ = 50.0  # the acquirer is sized once, not per sweep point


def detects(reps: int, snr_db: float, seed: int) -> bool:
    """One trial: does the acquirer find this burst at all?

    The acquirer is built at a FIXED design C/N0 while the sweep varies the
    true SNR, so no arm is ever handed the answer it is being tested on.
    """
    acq = BurstAcquisition(
        ACQ_CODE,
        reps=reps,
        spc=CHIP_SPS,
        chip_rate=FS / CHIP_SPS,
        cn0_dbhz=DESIGN_CN0_DBHZ,
        doppler_uncertainty=0.0,  # this demo has no carrier offset
        pfa=1e-3,
    )
    acq.configure_search_raw(reps, 1)
    burst_only = build_burst(
        acq_reps=reps,
        n_payload_sym=0,
        snr_db=snr_db,
        silence_chips=0,
        seed=seed,
    )
    return bool(acq.push(burst_only))


pd_curves: dict[int, np.ndarray] = {
    reps: np.array(
        [
            sum(detects(reps, snr, s) for s in range(SWEEP_TRIALS))
            / SWEEP_TRIALS
            for snr in SWEEP_SNRS
        ]
    )
    for reps in REPS_SWEEP
}


def pd50(curve: np.ndarray) -> float:
    """Input SNR where Pd first crosses 0.5, linearly interpolated."""
    for i in range(1, len(curve)):
        if curve[i - 1] < 0.5 <= curve[i]:
            f = (0.5 - curve[i - 1]) / (curve[i] - curve[i - 1])
            return float(
                SWEEP_SNRS[i - 1] + f * (SWEEP_SNRS[i] - SWEEP_SNRS[i - 1])
            )
    return float("nan")


thresholds = {reps: pd50(pd_curves[reps]) for reps in REPS_SWEEP}
shifts = [
    thresholds[a] - thresholds[b] for a, b in zip(REPS_SWEEP, REPS_SWEEP[1:])
]

# ── validate the sweep: the detection threshold moves, and by how much ───────
# The claim is a RATIO between arms, so the check has to be one too. The old
# assertion compared each arm to a floor, which one period of a 127-chip code
# clears on its own -- so it passed while the reps axis did nothing at all.
#
# Ideal is 10*log10(2) = 3.01 dB per doubling. Measured: 2.26, 2.50, 3.12 dB
# (thresholds -13.6, -15.9, -18.4, -21.5 dB), total 7.9 dB against an ideal
# 9.03. The shortfall is real and worth knowing rather than tuning away: more
# coherent depth means more search cells, so the Bonferroni threshold rises
# with it (the object reports 3.98 -> 4.30 across these four arms). That
# accounts for part of the gap; this file does not claim it accounts for all
# of it.
assert all(np.isfinite(t) for t in thresholds.values()), (
    "a reps arm never reached Pd = 0.5 -- widen SWEEP_SNRS"
)
assert all(s > 0 for s in shifts), (
    f"the detection threshold did not fall with acq_reps: {thresholds}"
)
assert all(1.5 <= s <= 4.0 for s in shifts), (
    f"per-doubling shift outside [1.5, 4.0] dB of the 3.01 dB ideal: {shifts}"
)
total = thresholds[REPS_SWEEP[0]] - thresholds[REPS_SWEEP[-1]]
assert 6.5 <= total <= 9.5, (
    f"total 1->8 shift {total:.1f} dB, expected near 10*log10(8) = 9.03"
)
print(
    "detection threshold (Pd=0.5): "
    + ", ".join(f"{r} reps -> {thresholds[r]:.1f} dB" for r in REPS_SWEEP)
)
print(
    f"  shift per doubling: {', '.join(f'{v:.2f}' for v in shifts)} dB "
    f"(ideal 3.01); total {total:.2f} dB (ideal 9.03)"
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

# ── D: Pd vs input SNR — the detection threshold moving with acq_reps ────────
ax = axes[1, 1]
colors = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728"]
for reps, color in zip(REPS_SWEEP, colors):
    ax.plot(
        SWEEP_SNRS,
        pd_curves[reps],
        lw=1.5,
        color=color,
        label=f"{reps} rep{'s' if reps > 1 else ''}",
    )
    # The Pd=0.5 crossing is the "detection threshold" the claim is about,
    # so mark the number the assertion actually tests.
    ax.axvline(thresholds[reps], color=color, lw=0.8, ls=":", alpha=0.7)
ax.axhline(0.5, color="k", lw=0.8, ls="--", alpha=0.6)
ax.set_xlabel("input SNR (dB, full band)")
ax.set_ylabel("measured $P_d$")
ax.set_ylim(-0.03, 1.03)
ax.set_title(
    f"Detection probability vs input SNR — {SWEEP_TRIALS} trials/point\n"
    f"threshold moves {np.mean(shifts):.1f} dB per doubling "
    "(ideal 10·log10(2) = 3.0)",
    fontsize=9,
)
ax.legend(title="acq_reps", fontsize=8, loc="upper left")
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
    f"  payload     : {payload_ms:.2f} ms  ({N_PAYLOAD_SYM} BPSK syms, "
    f"{DATA_SF} chips/bit)\n"
    f"  silence     : {silence_ms:.2f} ms  ({SILENCE_CHIPS} chips)\n"
    f"  SNR         : {SNR_DB:.0f} dB  →  wrote dsss_burst_demo.png"
)
