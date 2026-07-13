"""hbdecim_q15_demo.py — Q15 halfband decimator gallery figure.

Three-panel dark-theme figure:

  Panel 1 (top, full width)
    Frequency response of the Q15 halfband filter vs the float32 reference.
    Measured via impulse → filter → FFT (exercises the actual C code).

  Panel 2 (bottom-left)
    Input spectrum: wideband noise + passband tone (f=0.08) + stopband
    tone (f=0.35), all as interleaved int16 IQ.

  Panel 3 (bottom-right)
    Output spectrum after HalfbandDecimatorQ15.execute() — passband tone
    shifts to f=0.16 of the output rate; stopband tone is suppressed ~60 dB.

Run:
  python examples/python/hbdecim_q15_demo.py
"""

import matplotlib

matplotlib.use("Agg")
from typing import Optional

import matplotlib.pyplot as plt

# --8<-- [start:usage]
import numpy as np

from doppler.resample import HalfbandDecimatorQ15, _halfband_bank

# ── design ─────────────────────────────────────────────────────────────
bank = _halfband_bank(atten=60.0, pb=0.4, sb=0.6)
fir_row = int(np.argmin([np.max(np.abs(bank[r])) for r in range(2)]))
h = bank[fir_row].astype(np.float32)

# ── create ─────────────────────────────────────────────────────────────
dec = HalfbandDecimatorQ15(h)
print(f"num_taps={dec.num_taps}, rate={dec.rate}")  # 19, 0.5

# ── generate IQ signal: two tones, interleaved int16 ───────────────────
fs_in = 1.0  # normalised; 1 sample/cycle
N = 4096
t = np.arange(N)
amp = 20000  # -4 dBFS (int16 full scale = 32767)
f_pass = 0.05  # in passband (0 → 0.4 × fs_in/2)
f_stop = 0.35  # in stopband (0.6 × fs_in/2 → fs_in/2)

x_c = amp * np.exp(2j * np.pi * f_pass * t) + amp * 0.1 * np.exp(
    2j * np.pi * f_stop * t
)
x_iq = np.empty(2 * N, dtype=np.int16)
x_iq[0::2] = x_c.real.astype(np.int16)
x_iq[1::2] = x_c.imag.astype(np.int16)

# ── execute ────────────────────────────────────────────────────────────
# execute() returns a zero-copy view; copy it before the next call
# overwrites it.
y_iq = dec.execute(x_iq).copy()
print(f"output length: {len(y_iq) // 2} complex samples")  # 2048

# ── decode ─────────────────────────────────────────────────────────────
settle = dec.num_taps
y_c = y_iq[0::2].astype(np.float64) + 1j * y_iq[1::2].astype(np.float64)

# ── measure passband amplitude after filter settles ────────────────────
w = np.hanning(len(y_c) - settle)
S = np.abs(np.fft.fft(y_c[settle:] * w))
pb_amp = np.max(S) / ((len(y_c) - settle) * w.mean())
print(f"passband amplitude: {pb_amp / amp:.4f}  (expect ≈ 1.0)")
assert abs(pb_amp / amp - 1.0) < 0.05

# ── streaming: feed in blocks, copy each result ────────────────────────
dec.reset()
chunk = 128  # 64 IQ pairs per call
results = []
for i in range(0, len(x_iq), chunk):
    results.append(dec.execute(x_iq[i : i + chunk]).copy())
y_stream = np.concatenate(results)
assert np.array_equal(y_iq, y_stream)  # block-split == one-shot ✓

# ── context manager ────────────────────────────────────────────────────
with HalfbandDecimatorQ15(h) as d:
    y2 = d.execute(x_iq).copy()

# ── explicit destroy ───────────────────────────────────────────────────
dec.destroy()
# --8<-- [end:usage]

# ---------------------------------------------------------------------------
# theme constants
# ---------------------------------------------------------------------------

BG_FIG = "#0f172a"
BG_AXES = "#111827"
C_GRID = "#374151"
C_TEXT = "#d1d5db"
C_SPINE = "#374151"

C_Q15 = "#60a5fa"  # blue — Q15 implementation
C_FLOAT = "#94a3b8"  # gray — float32 reference
C_PASS = "#14532d"  # dark green — passband region
C_TRANS = "#713f12"  # dark amber — transition region
C_STOP = "#450a0a"  # dark red  — stopband region
C_TONE_P = "#4ade80"  # green — passband tone
C_TONE_S = "#f87171"  # red   — stopband tone


# ---------------------------------------------------------------------------
# coefficients
# ---------------------------------------------------------------------------


def _get_coeffs() -> np.ndarray:
    """Return the non-trivial FIR polyphase branch of the halfband bank.

    ``_halfband_bank`` returns a 2×N polyphase bank; one row contains only
    one non-zero centre coefficient (the delay branch) while the other row
    holds all the non-trivial taps.  We select the non-trivial row and
    cast to float32 — the same format ``HalfbandDecimatorQ15`` expects.
    """
    bank = _halfband_bank(60.0, 0.4, 0.6)
    centre = bank.shape[1] // 2
    fir_row = (
        0 if abs(float(bank[0, centre])) < abs(float(bank[1, centre])) else 1
    )
    return np.ascontiguousarray(bank[fir_row]).astype(np.float32)


# ---------------------------------------------------------------------------
# frequency-response measurement (impulse method)
# ---------------------------------------------------------------------------


def _measure_freq_response_q15(
    h: np.ndarray,
    n_impulse: int = 32768,
    pad: int = 4,
) -> tuple[np.ndarray, np.ndarray]:
    """Measure HalfbandDecimatorQ15 frequency response, two-phase impulse.

    One impulse per polyphase branch is injected as interleaved int16 IQ
    (I=amplitude at sample d ∈ {0, 1}, Q=0 throughout); the two decimated
    outputs are re-interleaved into the full-rate impulse response, whose
    FFT is the true response.  Amplitude is normalised so 0 dBFS = unity
    filter gain.

    The input sample rate is 1.0; the response covers [0, 0.5) of that
    rate (one-sided, since the input is real-valued IQ with I only).

    Parameters
    ----------
    h : np.ndarray
        FIR taps (float32) for HalfbandDecimatorQ15.
    n_impulse : int
        Number of complex input samples.  Longer → finer frequency
        resolution.
    pad : int
        Zero-padding multiplier for the output FFT.

    Returns
    -------
    freq : np.ndarray
        Normalised frequencies [0, 0.5) of the *input* rate.
    mag_db : np.ndarray
        Log magnitude in dBFS (reference = 32768).
    """
    amplitude = 16384  # half full-scale so headroom avoids int16 clipping

    # A 2:1 decimator computes y[m] = sum_k h[k] x[2m-k], so an impulse
    # at input sample d excites exactly one polyphase branch:
    # y[m] = h[2m-d].  An impulse at d=0 alone returns only the even
    # taps -- for a halfband that is the pure center-tap delay branch,
    # which measures as a flat line, not the filter.  Inject at d=0 and
    # d=1 and re-interleave the two outputs to reconstruct the full tap
    # set; its FFT is the true response at the *input* rate.
    phases = []
    for d in (0, 1):
        x_iq = np.zeros(2 * n_impulse, dtype=np.int16)
        x_iq[2 * d] = amplitude  # I-channel impulse at sample d
        dec = HalfbandDecimatorQ15(h)
        y_iq = dec.execute(x_iq)
        dec.destroy()
        phases.append(
            y_iq[0::2].astype(np.float64) + 1j * y_iq[1::2].astype(np.float64)
        )
    y0, y1 = phases

    # y0[m] = h[2m], y1[m] = h[2m-1]  ->  h[2m+1] = y1[m+1]
    n_pair = min(len(y0), len(y1)) - 1
    h_rec = np.empty(2 * n_pair, dtype=np.complex128)
    h_rec[0::2] = y0[:n_pair]
    h_rec[1::2] = y1[1 : n_pair + 1]

    S = np.fft.fft(h_rec, 2 * n_pair * pad)
    half = (2 * n_pair * pad) // 2
    # 0 dBFS = unity filter gain (impulse height normalised out)
    mag = np.abs(S[:half]) / (amplitude / 32768.0) / 32768.0
    mag_db = 20.0 * np.log10(mag + 1e-12)

    # Frequency axis in *input* normalised frequency [0, 0.5)
    freq = np.linspace(0.0, 0.5, half, endpoint=False)
    return freq, mag_db


def _measure_freq_response_float(
    h: np.ndarray,
    n_impulse: int = 32768,
    pad: int = 4,
) -> tuple[np.ndarray, np.ndarray]:
    """Measure HalfbandDecimator (float32) frequency response via impulse.

    Same methodology as ``_measure_freq_response_q15`` but using the float32
    reference decimator.  Input is a complex64 unit impulse.

    Parameters
    ----------
    h : np.ndarray
        FIR taps (float32).
    n_impulse : int
        Number of complex input samples.
    pad : int
        Zero-padding multiplier for the output FFT.

    Returns
    -------
    freq : np.ndarray
        Normalised frequencies [0, 0.5) of the input rate.
    mag_db : np.ndarray
        Log magnitude in dBFS (0 dBFS = unity gain).
    """
    from doppler.resample import HalfbandDecimator

    # Two-phase impulse reconstruction -- see the Q15 twin above for why
    # a single impulse cannot measure a decimator's response.
    phases = []
    for d in (0, 1):
        x_c = np.zeros(n_impulse, dtype=np.complex64)
        x_c[d] = 1.0 + 0j
        dec = HalfbandDecimator(h)
        y = dec.execute(x_c)
        dec.destroy()
        phases.append(y.astype(np.complex128))
    y0, y1 = phases

    # y0[m] = h[2m], y1[m] = h[2m-1]  ->  h[2m+1] = y1[m+1]
    n_pair = min(len(y0), len(y1)) - 1
    h_rec = np.empty(2 * n_pair, dtype=np.complex128)
    h_rec[0::2] = y0[:n_pair]
    h_rec[1::2] = y1[1 : n_pair + 1]

    S = np.fft.fft(h_rec, 2 * n_pair * pad)
    half = (2 * n_pair * pad) // 2
    mag = np.abs(S[:half])
    mag_db = 20.0 * np.log10(mag + 1e-12)

    freq = np.linspace(0.0, 0.5, half, endpoint=False)
    return freq, mag_db


# ---------------------------------------------------------------------------
# input/output signal spectra
# ---------------------------------------------------------------------------


def _bh_spectrum_db(
    x_c: np.ndarray,
    full_scale: float,
    pad: int = 4,
) -> tuple[np.ndarray, np.ndarray]:
    """One-sided Blackman-Harris windowed spectrum in dBFS.

    Parameters
    ----------
    x_c : np.ndarray
        Complex input signal.
    full_scale : float
        Reference amplitude for 0 dBFS.
    pad : int
        Zero-padding multiplier.

    Returns
    -------
    freq : np.ndarray
        Normalised frequencies [0, 0.5).
    mag_db : np.ndarray
        Magnitude in dBFS.
    """
    from doppler.spectral import blackman_harris_window

    n = len(x_c)
    w = np.zeros(n, dtype=np.float32)
    blackman_harris_window(w)
    cg = float(w.mean())

    S = np.fft.fft(x_c * w, n * pad)
    half = (n * pad) // 2
    mag = np.abs(S[:half]) / (n * cg * full_scale)
    mag_db = 20.0 * np.log10(mag + 1e-12)

    freq = np.linspace(0.0, 0.5, half, endpoint=False)
    return freq, mag_db


def _build_input_signal(
    n: int = 8192,
    rng: Optional[np.random.Generator] = None,
) -> np.ndarray:
    """Return interleaved int16 IQ wideband test signal.

    Signal components (all at int16 scale, full-scale = 32768):
      - White noise at amplitude 300  (~-40 dBFS RMS)
      - Passband tone f0=0.08 at amplitude 6000 (~-15 dBFS peak)
      - Stopband tone f0=0.35 at amplitude 6000 (~-15 dBFS peak)

    Parameters
    ----------
    n : int
        Number of complex (IQ) samples.
    rng : np.random.Generator or None
        RNG for reproducible noise.

    Returns
    -------
    x_iq : np.ndarray, dtype=int16
        Interleaved IQ: [I0, Q0, I1, Q1, ...], length = 2*n.
    """
    if rng is None:
        rng = np.random.default_rng(0)

    t = np.arange(n, dtype=np.float64)

    # White noise
    noise_i = (300.0 * rng.standard_normal(n)).astype(np.float64)
    noise_q = (300.0 * rng.standard_normal(n)).astype(np.float64)

    # Passband tone at f0=0.08
    amp_tone = 6000.0
    f_pass = 0.08
    pass_i = amp_tone * np.cos(2 * np.pi * f_pass * t)
    pass_q = amp_tone * np.sin(2 * np.pi * f_pass * t)

    # Stopband tone at f0=0.35
    f_stop = 0.35
    stop_i = amp_tone * np.cos(2 * np.pi * f_stop * t)
    stop_q = amp_tone * np.sin(2 * np.pi * f_stop * t)

    sig_i = (noise_i + pass_i + stop_i).clip(-32768, 32767).astype(np.int16)
    sig_q = (noise_q + pass_q + stop_q).clip(-32768, 32767).astype(np.int16)

    return np.stack([sig_i, sig_q], axis=1).ravel()


# ---------------------------------------------------------------------------
# axes styling helper
# ---------------------------------------------------------------------------


def _style(ax: plt.Axes) -> None:
    ax.set_facecolor(BG_AXES)
    ax.grid(True, color=C_GRID, lw=0.4)
    ax.tick_params(colors=C_TEXT)
    for sp in ax.spines.values():
        sp.set_color(C_SPINE)
    ax.xaxis.label.set_color(C_TEXT)
    ax.yaxis.label.set_color(C_TEXT)
    ax.title.set_color(C_TEXT)


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------


def main(out_path: str = "hbdecim_q15_demo.png") -> None:
    """Produce the three-panel gallery figure and save to *out_path*.

    Parameters
    ----------
    out_path : str
        Destination filename for the PNG output.
    """
    h = _get_coeffs()

    # ── frequency response measurements ──────────────────────────────────────
    freq_q15, mag_q15 = _measure_freq_response_q15(h)
    freq_f32, mag_f32 = _measure_freq_response_float(h)

    # Validate the measured response is a real halfband shape (a
    # one-phase impulse measurement degenerates to a flat line -- the
    # bug this two-phase method replaced): flat passband, deep stopband,
    # and the Q15 path within quantization distance of the float32
    # reference across the passband.
    for tag, f, m in (("q15", freq_q15, mag_q15), ("f32", freq_f32, mag_f32)):
        pb_dev = float(np.max(np.abs(m[f <= 0.20])))
        sb_max = float(np.max(m[f >= 0.30]))
        print(
            f"  {tag}: passband dev {pb_dev:.3f} dB, "
            f"stopband max {sb_max:.1f} dB"
        )
        assert pb_dev < 0.1, f"{tag} passband not flat: {pb_dev:.3f} dB"
        assert sb_max < -55.0, f"{tag} stopband only {sb_max:.1f} dB"
    pb = freq_q15 <= 0.20
    q15_vs_f32 = float(np.max(np.abs(mag_q15[pb] - mag_f32[pb])))
    assert q15_vs_f32 < 0.05, (
        f"Q15 deviates {q15_vs_f32:.3f} dB from float32 in the passband"
    )

    # ── input/output signals ─────────────────────────────────────────────────
    rng = np.random.default_rng(42)
    x_iq = _build_input_signal(n=8192, rng=rng)

    # Input spectrum from complex view
    x_c = x_iq[0::2].astype(np.float64) + 1j * x_iq[1::2].astype(np.float64)
    freq_in, mag_in = _bh_spectrum_db(x_c, full_scale=32768.0)

    # Run through HalfbandDecimatorQ15
    dec = HalfbandDecimatorQ15(h)
    settle = dec.num_taps // 2
    y_iq = dec.execute(x_iq)
    dec.destroy()

    y_c = y_iq[0::2].astype(np.float64) + 1j * y_iq[1::2].astype(np.float64)
    y_c_settled = y_c[settle:]

    freq_out, mag_out = _bh_spectrum_db(y_c_settled, full_scale=32768.0)

    # ── validation ───────────────────────────────────────────────────────────
    # 2:1 decimation: exactly half the complex samples come out.
    assert len(y_c) == len(x_c) // 2, (
        f"expected {len(x_c) // 2} output samples, got {len(y_c)}"
    )

    # The passband tone (f=0.08 in) must dominate the output spectrum at
    # f=0.16 of the output rate, having crossed the filter at unity gain.
    tone_in_db = float(mag_in[int(np.argmin(np.abs(freq_in - 0.08)))])
    tone_out_db = float(mag_out[int(np.argmin(np.abs(freq_out - 0.16)))])
    peak_f = float(freq_out[int(np.argmax(mag_out))])
    assert abs(peak_f - 0.16) < 0.005, (
        f"output peak at f={peak_f:.4f}, expected 0.16"
    )
    assert abs(tone_out_db - tone_in_db) < 1.0, (
        f"passband tone gain {tone_out_db - tone_in_db:+.2f} dB not unity"
    )

    # The stopband tone entered at the same -15 dBFS as the passband tone;
    # after the halfband nothing but the noise floor may remain anywhere
    # else in the output band (>= 40 dB below the surviving tone).
    elsewhere = (np.abs(freq_out - 0.16) > 0.04) & (freq_out > 0.02)
    residual_db = float(np.max(mag_out[elsewhere]))
    suppression = tone_out_db - residual_db
    assert suppression > 40.0, (
        f"stopband residual {residual_db:.1f} dBFS — only "
        f"{suppression:.1f} dB below the passband tone"
    )
    print(
        f"validation: tone {tone_in_db:.1f} → {tone_out_db:.1f} dBFS at "
        f"f={peak_f:.3f}; worst residual {suppression:.1f} dB down — OK"
    )

    # ── figure layout ────────────────────────────────────────────────────────
    fig = plt.figure(figsize=(14, 12), facecolor=BG_FIG)
    gs = fig.add_gridspec(
        2,
        2,
        height_ratios=[1.1, 1.0],
        hspace=0.38,
        wspace=0.28,
        left=0.07,
        right=0.97,
        top=0.93,
        bottom=0.06,
    )

    ax_resp = fig.add_subplot(gs[0, :])  # full-width top panel
    ax_in = fig.add_subplot(gs[1, 0])  # bottom-left
    ax_out = fig.add_subplot(gs[1, 1])  # bottom-right

    # ── Panel 1: frequency response ──────────────────────────────────────────

    # Background region shading
    ax_resp.axvspan(0.00, 0.20, color=C_PASS, alpha=0.35, label="_pass bg")
    ax_resp.axvspan(0.20, 0.30, color=C_TRANS, alpha=0.35, label="_trans bg")
    ax_resp.axvspan(0.30, 0.50, color=C_STOP, alpha=0.35, label="_stop bg")

    # Region labels (text near top)
    ax_resp.text(
        0.10,
        1.5,
        "Passband",
        ha="center",
        va="bottom",
        color="#86efac",
        fontsize=9,
    )
    ax_resp.text(
        0.25,
        1.5,
        "Transition",
        ha="center",
        va="bottom",
        color="#fde68a",
        fontsize=9,
    )
    ax_resp.text(
        0.40,
        1.5,
        "Stopband",
        ha="center",
        va="bottom",
        color="#fca5a5",
        fontsize=9,
    )

    ax_resp.plot(
        freq_f32,
        mag_f32,
        color=C_FLOAT,
        lw=1.0,
        ls="--",
        label="Float32 reference (HalfbandDecimator)",
    )
    ax_resp.plot(
        freq_q15,
        mag_q15,
        color=C_Q15,
        lw=1.2,
        label="Q15 (HalfbandDecimatorQ15)",
    )

    # Passband / stopband edge annotations
    ax_resp.axvline(0.20, color="#86efac", lw=0.8, ls=":", alpha=0.8)
    ax_resp.axvline(0.30, color="#fca5a5", lw=0.8, ls=":", alpha=0.8)
    ax_resp.text(
        0.20,
        -84,
        "0.20",
        ha="center",
        va="bottom",
        color="#86efac",
        fontsize=8,
    )
    ax_resp.text(
        0.30,
        -84,
        "0.30",
        ha="center",
        va="bottom",
        color="#fca5a5",
        fontsize=8,
    )

    ax_resp.set_xlim(0.0, 0.5)
    ax_resp.set_ylim(-90, 3)
    ax_resp.set_xlabel(
        "Normalised input frequency (cycles/sample)", fontsize=9
    )
    ax_resp.set_ylabel("Magnitude (dBFS)", fontsize=9)
    ax_resp.set_title(
        "Halfband frequency response — Q15 vs float32", fontsize=11
    )
    ax_resp.legend(
        fontsize=9,
        facecolor="#1f2937",
        edgecolor="#4b5563",
        labelcolor=C_TEXT,
        loc="lower left",
    )
    _style(ax_resp)

    # ── Panel 2: input spectrum ──────────────────────────────────────────────

    ax_in.plot(freq_in, mag_in, color=C_Q15, lw=0.8)

    # Annotate passband tone
    f_pass_norm = 0.08
    idx_p = int(np.argmin(np.abs(freq_in - f_pass_norm)))
    tone_p_db = float(mag_in[idx_p])
    ax_in.annotate(
        "Passband tone\nf=0.08",
        xy=(f_pass_norm, tone_p_db),
        xytext=(f_pass_norm + 0.06, tone_p_db - 12),
        color=C_TONE_P,
        fontsize=8,
        arrowprops={"arrowstyle": "->", "color": C_TONE_P, "lw": 0.9},
    )

    # Annotate stopband tone
    f_stop_norm = 0.35
    idx_s = int(np.argmin(np.abs(freq_in - f_stop_norm)))
    tone_s_db = float(mag_in[idx_s])
    ax_in.annotate(
        "Stopband tone\nf=0.35",
        xy=(f_stop_norm, tone_s_db),
        xytext=(f_stop_norm - 0.13, tone_s_db - 14),
        color=C_TONE_S,
        fontsize=8,
        arrowprops={"arrowstyle": "->", "color": C_TONE_S, "lw": 0.9},
    )

    ax_in.set_xlim(0.0, 0.5)
    ax_in.set_ylim(-90, 3)
    ax_in.set_xlabel("Normalised frequency (cycles/sample)", fontsize=9)
    ax_in.set_ylabel("Magnitude (dBFS)", fontsize=9)
    ax_in.set_title("Input spectrum (before decimation)", fontsize=10)
    _style(ax_in)

    # ── Panel 3: output spectrum ─────────────────────────────────────────────

    ax_out.plot(freq_out, mag_out, color=C_Q15, lw=0.8)

    # Passband tone appears at f=0.16 of output rate (0.08 * 2 since 2:1 decim)
    f_pass_out = 0.16
    idx_po = int(np.argmin(np.abs(freq_out - f_pass_out)))
    tone_po_db = float(mag_out[idx_po])
    ax_out.annotate(
        "Passband tone\nf=0.16 (output rate)",
        xy=(f_pass_out, tone_po_db),
        xytext=(f_pass_out + 0.05, tone_po_db - 12),
        color=C_TONE_P,
        fontsize=8,
        arrowprops={"arrowstyle": "->", "color": C_TONE_P, "lw": 0.9},
    )

    # Stopband tone should be suppressed — annotate near f=0.35 of output
    # (wraps to f=0.35 of output = aliased) — find the residual peak
    f_stop_out = 0.35
    idx_so = int(np.argmin(np.abs(freq_out - f_stop_out)))
    tone_so_db = float(mag_out[idx_so])
    ax_out.annotate(
        "~-60 dB stopband\nsuppression",
        xy=(f_stop_out, tone_so_db),
        xytext=(f_stop_out - 0.14, tone_so_db + 18),
        color=C_TONE_S,
        fontsize=8,
        arrowprops={"arrowstyle": "->", "color": C_TONE_S, "lw": 0.9},
    )

    ax_out.set_xlim(0.0, 0.5)
    ax_out.set_ylim(-90, 3)
    ax_out.set_xlabel(
        "Normalised frequency (cycles/sample, output rate)", fontsize=9
    )
    ax_out.set_ylabel("Magnitude (dBFS)", fontsize=9)
    ax_out.set_title(
        "Output spectrum (after HalfbandDecimatorQ15, 2:1 decimation)",
        fontsize=10,
    )
    _style(ax_out)

    # ── save ─────────────────────────────────────────────────────────────────
    fig.suptitle(
        "HalfbandDecimatorQ15 — Q15 halfband 2:1 decimator",
        fontsize=13,
        color=C_TEXT,
        y=0.97,
    )
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved → {out_path}")


if __name__ == "__main__":
    main()
