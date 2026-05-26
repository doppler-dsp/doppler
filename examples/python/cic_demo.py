"""cic_demo.py — CIC decimation filter demo.

Shows:
  1. Basic decimation — output sample count and DC passthrough
  2. Alias rejection — passband vs alias-zone tones measured in dB
  3. Reconfigure — switch R at runtime and verify the new rate
  4. SDR front-end pipeline — wideband IQ → CIC → narrowband spectrum
  5. Spectral plot — input and output spectra saved to cic_demo_spectrum.png

A CIC filter is a cascade of N=4 integrators and N=4 comb sections
(M=1 differential delay) separated by R:1 downsampling.  The pipeline
is a fixed-point UQ16 integer path with no multiplications, making it
ideal as the first decimation stage at very high sample rates (R ≤ 4096).

Run:
  python examples/python/cic_demo.py
"""

from __future__ import annotations

import math

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import numpy as np

from doppler.resample import CIC


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def _tone(freq_norm: float, n: int) -> np.ndarray:
    """Complex exponential at freq_norm (cycles/sample)."""
    t = np.arange(n)
    return np.exp(2j * np.pi * freq_norm * t).astype(np.complex64)


def _rms_db(x: np.ndarray) -> float:
    rms = float(np.sqrt(np.mean(np.abs(x) ** 2)))
    return 20.0 * math.log10(rms + 1e-300)


def _decimate(cic: CIC, x: np.ndarray) -> np.ndarray:
    return np.array(cic.decimate(x), copy=True)


def _uq16_roundtrip(x: np.ndarray) -> np.ndarray:
    """Replicate the CIC UQ16 input: CF32 → offset-binary uint16 → CF32.

    Bipolar → unipolar: −1.0 → 0, 0.0 → 32768, +1.0 → 65535.
    """
    q15 = np.clip(np.real(x) * 32768.0, -32768, 32767).astype(np.int16)
    re_u16 = (q15.astype(np.int32) + 32768).astype(np.uint16)
    q15 = np.clip(np.imag(x) * 32768.0, -32768, 32767).astype(np.int16)
    im_u16 = (q15.astype(np.int32) + 32768).astype(np.uint16)
    re_f = (re_u16.astype(np.int32) - 32768).astype(np.float32) / 32768.0
    im_f = (im_u16.astype(np.int32) - 32768).astype(np.float32) / 32768.0
    return re_f + 1j * im_f


CIC_N = 4  # fixed stage count


def _settled(y: np.ndarray, R: int) -> np.ndarray:
    """Drop the initial transient (≈ CIC_N*(R-1)/R output samples)."""
    n_drop = CIC_N * (R - 1) // R + 2
    return y[n_drop:]


def _spectrum_db(x: np.ndarray, fs: float, pad: int = 8,
                 full: bool = False) -> tuple[np.ndarray, np.ndarray]:
    """Windowed FFT spectrum.

    Returns (freq_hz, amplitude_db) using a Blackman-Harris window and
    zero-padding by `pad` for smoother display.  The amplitude is normalised
    so a unit-amplitude complex tone reads 0 dBFS.

    Parameters
    ----------
    full : bool
        If False (default), return only 0…fs/2.
        If True, return the full −fs/2…+fs/2 range (useful for complex IQ).
    """
    n = len(x)
    a = [0.35875, 0.48829, 0.14128, 0.01168]
    k = 2 * np.pi * np.arange(n) / n
    w = a[0] - a[1]*np.cos(k) + a[2]*np.cos(2*k) - a[3]*np.cos(3*k)
    cg = w.mean()
    S = np.fft.fft(x * w, n * pad)
    amp_db = 20.0 * np.log10(np.abs(S) / (n * cg) + 1e-300)
    freq_hz = np.fft.fftfreq(n * pad, d=1.0 / fs)
    if full:
        order = np.argsort(freq_hz)
        return freq_hz[order], amp_db[order]
    pos = freq_hz >= 0
    return freq_hz[pos], amp_db[pos]


def _cic_response_db(freq_hz: np.ndarray, fs: float,
                     R: int, N: int, M: int = 1) -> np.ndarray:
    """CIC magnitude response in dB at the given frequencies.

    |H(f)| = (1/R) × |sin(π f M R / fs) / sin(π f M / fs)|^N

    DC gain is normalised to 0 dB (unity passband reference).
    """
    with np.errstate(divide="ignore", invalid="ignore"):
        arg_num = np.pi * freq_hz * M * R / fs
        arg_den = np.pi * freq_hz * M / fs
        h = np.where(
            np.abs(arg_den) < 1e-12,
            1.0,
            np.abs(np.sin(arg_num) / (R * np.sin(arg_den))) ** N,
        )
    return 20.0 * np.log10(h + 1e-300)


# ---------------------------------------------------------------------------
# 1. Basic decimation
# ---------------------------------------------------------------------------

def demo_basic():
    print("--- 1. Basic decimation (R=8, N=4, M=1) ---")
    R = 8
    cic = CIC(R)

    print(f"  R={cic.R}  N={CIC_N}  M=1  shift={cic.shift}")

    n_in = 12 * R * CIC_N
    x = np.ones(n_in, dtype=np.complex64)
    y = _decimate(cic, x)
    settled = _settled(y, R)
    dc_rms = float(np.mean(np.abs(settled)))
    print(f"  DC input → settled output magnitude: {dc_rms:.6f} (expect 1.0)")
    print(f"  output samples: {len(y)}  (= {n_in} / {R})\n")


# ---------------------------------------------------------------------------
# 2. Alias rejection
# ---------------------------------------------------------------------------

def demo_alias_rejection():
    print("--- 2. Alias rejection (R=8, N=4, M=1) ---")
    R = 8

    f_pass  = 0.1 / (2 * R)
    f_alias = 0.95 / R

    n_in = 32 * R * CIC_N
    x_pass  = _tone(f_pass,  n_in)
    x_alias = _tone(f_alias, n_in)

    cic = CIC(R)
    y_pass  = _settled(_decimate(cic, x_pass),  R)
    cic.reset()
    y_alias = _settled(_decimate(cic, x_alias), R)

    pass_db  = _rms_db(y_pass)
    alias_db = _rms_db(y_alias)
    rejection = pass_db - alias_db

    sinc_atten = 20.0 * CIC_N * math.log10(
        math.pi * f_alias * R / math.sin(math.pi * f_alias * R) + 1e-300
    )
    print(f"  passband tone   f={f_pass:.4f}*fs  → {pass_db:+.1f} dBFS")
    print(f"  alias-zone tone f={f_alias:.4f}*fs  → {alias_db:+.1f} dBFS")
    print(f"  alias rejection: {rejection:.1f} dB")
    print(f"  theoretical:     {sinc_atten:.1f} dB\n")


# ---------------------------------------------------------------------------
# 3. Reconfigure at runtime
# ---------------------------------------------------------------------------

def demo_reconfigure():
    print("--- 3. Runtime reconfigure ---")
    cic = CIC(4)
    print(f"  initial   R={cic.R}  shift={cic.shift}")

    x = np.ones(32, dtype=np.complex64)
    y = cic.decimate(x)
    print(f"  32 samples → {len(y)} output (R=4)")

    cic.reconfigure(8)
    print(f"  reconfigured R={cic.R}  shift={cic.shift}")

    x = np.ones(64, dtype=np.complex64)
    y = cic.decimate(x)
    print(f"  64 samples → {len(y)} output (R=8)\n")


# ---------------------------------------------------------------------------
# 4. SDR front-end pipeline
# ---------------------------------------------------------------------------

def demo_sdr_pipeline():
    """Simulate a wideband IQ signal and decimate it to a narrowband slice."""
    print("--- 4. SDR front-end pipeline ---")

    fs_in  = 2.048e6
    R      = 16
    fs_out = fs_in / R

    f_wanted = 15e3
    f_jammer = 208e3   # first alias zone [fs_out, 2*fs_out); aliases to 48 kHz

    fn_wanted = f_wanted / fs_in
    fn_jammer = f_jammer / fs_in

    # Scale so the two-tone sum stays within ±1.0 per component.
    # Worst-case component amplitude = A_wanted + A_jammer; use 0.6 + 0.3 = 0.9.
    A_wanted = 0.6
    A_jammer = 0.3

    N_IN  = 8 * R * 48
    x = A_wanted * _tone(fn_wanted, N_IN) + A_jammer * _tone(fn_jammer, N_IN)

    print(f"  Input fs = {fs_in/1e6:.3f} Msps, R={R} → output {fs_out/1e3:.0f} ksps")
    print(f"  Wanted:  {f_wanted/1e3:.0f} kHz  (fn={fn_wanted:.5f})")
    print(f"  Jammer:  {f_jammer/1e3:.0f} kHz  (fn={fn_jammer:.5f})")

    cic = CIC(R)
    y = _decimate(cic, x)
    settled = _settled(y, R)

    cic.reset()
    y_ref = _settled(_decimate(cic, A_wanted * _tone(fn_wanted, N_IN)), R)

    wanted_db  = _rms_db(y_ref)
    combined_db = _rms_db(settled)

    print(f"  Wanted-only output RMS:   {wanted_db:+.1f} dBFS")
    print(f"  Combined output RMS:      {combined_db:+.1f} dBFS")
    # Without CIC: power of two incoherent tones adds → 10·log₁₀(1 + (A_j/A_w)²)
    no_filter_excess_db = 10.0 * math.log10(1.0 + (A_jammer / A_wanted) ** 2)
    print(
        f"  Jammer visible as excess: {combined_db - wanted_db:+.1f} dB "
        f"(would be +{no_filter_excess_db:.1f} dB without CIC filtering)"
    )
    print()

    return x, settled, fs_in, fs_out, R, f_wanted, f_jammer


# ---------------------------------------------------------------------------
# 5. Spectral plot
# ---------------------------------------------------------------------------

def demo_spectral_plot(x, y_settled, fs_in, fs_out, R,
                       f_wanted, f_jammer, out_path="cic_demo_spectrum.png"):
    """Save a two-panel spectral plot: input (wideband) and output (decimated).

    Top panel — input spectrum with:
      - CIC |H(f)| magnitude response overlaid
      - Output Nyquist boundary marked
      - Both tone frequencies annotated

    Bottom panel — output spectrum with:
      - Wanted tone annotated (survived)
      - Jammer aliased-and-attenuated position annotated
    """
    fig, (ax_in, ax_q15, ax_out) = plt.subplots(
        3, 1, figsize=(10, 10), constrained_layout=True
    )
    fig.suptitle(
        f"CIC Decimation  R={R}, N={CIC_N}, M=1\n"
        f"Input {fs_in/1e6:.3f} Msps → Output {fs_out/1e3:.0f} ksps",
        fontsize=12,
        color="#f1f5f9",
    )

    # ── input spectrum ───────────────────────────────────────────────────────
    freq_in, amp_in = _spectrum_db(x, fs_in, full=True)
    freq_in_khz = freq_in / 1e3
    ax_in.plot(freq_in_khz, amp_in, color="#60a5fa", lw=0.8, zorder=2)

    # CIC response overlay (symmetric for real coefficients)
    h_db = _cic_response_db(freq_in, fs_in, R, CIC_N)
    ax_in.plot(freq_in_khz, h_db, color="#f97316", lw=1.5,
               linestyle="--", label=f"|H(f)| CIC N={CIC_N}", zorder=3)

    # Output Nyquist boundaries (±)
    nyq_out = fs_out / 2 / 1e3
    ax_in.axvline( nyq_out, color="#ffffff", lw=1.2, linestyle=":",
                   label=f"Output Nyquist (±{nyq_out:.0f} kHz)", zorder=4)
    ax_in.axvline(-nyq_out, color="#ffffff", lw=1.2, linestyle=":", zorder=4)

    # Tone annotations (complex exponentials: single-sided at +f)
    f_wanted_khz = f_wanted / 1e3
    f_jammer_khz = f_jammer / 1e3
    wanted_peak_in = float(amp_in[int(np.argmin(np.abs(freq_in_khz - f_wanted_khz)))])
    jammer_peak_in = float(amp_in[int(np.argmin(np.abs(freq_in_khz - f_jammer_khz)))])

    ax_in.axvline(f_wanted_khz, color="#4ade80", lw=1.0, linestyle="-.", alpha=0.8)
    ax_in.annotate(
        f"Wanted\n{f_wanted_khz:.0f} kHz",
        xy=(f_wanted_khz, wanted_peak_in), xytext=(-500, -15),
        color="#4ade80", fontsize=11, va="top",
        arrowprops=dict(arrowstyle="->", color="#4ade80", lw=1.2),
    )
    ax_in.axvline(f_jammer_khz, color="#f87171", lw=1.0, linestyle="-.", alpha=0.8)
    ax_in.annotate(
        f"Jammer\n{f_jammer_khz:.0f} kHz",
        xy=(f_jammer_khz, jammer_peak_in), xytext=(600, -45),
        color="#f87171", fontsize=11, va="top",
        arrowprops=dict(arrowstyle="->", color="#f87171", lw=1.2),
    )

    ax_in.set_xlim(-fs_in / 2 / 1e3, fs_in / 2 / 1e3)
    ax_in.set_ylim(-120, 10)
    ax_in.set_xlabel("Frequency (kHz)")
    ax_in.set_ylabel("Amplitude (dBFS)")
    ax_in.set_title("Input spectrum", loc="right", color="#f1f5f9")
    ax_in.legend(loc="upper right", fontsize=11,
                 facecolor="#1f2937", edgecolor="#4b5563",
                 labelcolor="#d1d5db")
    ax_in.grid(True, color="#374151", lw=0.4)
    ax_in.set_facecolor("#111827")
    ax_in.tick_params(colors="#d1d5db")
    for sp in ax_in.spines.values():
        sp.set_color("#374151")

    # ── Q15 roundtrip: CIC integer input converted back to CF32 ─────────────
    x_q15 = _uq16_roundtrip(x)
    freq_q15, amp_q15 = _spectrum_db(x_q15, fs_in, full=True)
    freq_q15_khz = freq_q15 / 1e3
    ax_q15.plot(freq_q15_khz, amp_q15, color="#60a5fa", lw=0.8, zorder=2)

    ax_q15.axvline( f_wanted / 1e3, color="#4ade80", lw=1.0, linestyle="-.", alpha=0.8)
    ax_q15.axvline( f_jammer / 1e3, color="#f87171", lw=1.0, linestyle="-.", alpha=0.8)
    ax_q15.axvline( nyq_out,        color="#ffffff",  lw=1.2, linestyle=":", zorder=4)
    ax_q15.axvline(-nyq_out,        color="#ffffff",  lw=1.2, linestyle=":", zorder=4)

    ax_q15.set_xlim(-fs_in / 2 / 1e3, fs_in / 2 / 1e3)
    ax_q15.set_ylim(-120, 10)
    ax_q15.set_xlabel("Frequency (kHz)")
    ax_q15.set_ylabel("Amplitude (dBFS)")
    ax_q15.set_title("Q15 quantized input (UQ16 → CF32 roundtrip)",
                     loc="right", color="#f1f5f9")
    ax_q15.grid(True, color="#374151", lw=0.4)
    ax_q15.set_facecolor("#111827")
    ax_q15.tick_params(colors="#d1d5db")
    for sp in ax_q15.spines.values():
        sp.set_color("#374151")

    # ── output spectrum (full complex: -fs_out/2 … +fs_out/2) ───────────────
    freq_out, amp_out = _spectrum_db(y_settled, fs_out, full=True)
    freq_out_khz = freq_out / 1e3
    ax_out.plot(freq_out_khz, amp_out, color="#60a5fa", lw=0.8, zorder=2)

    # Wanted tone in output coordinates
    f_wanted_out_khz = f_wanted / 1e3
    wanted_peak = float(amp_out[int(np.argmin(np.abs(freq_out_khz - f_wanted_out_khz)))])
    ax_out.axvline(f_wanted_out_khz, color="#4ade80",
                   lw=1.0, linestyle="-.", alpha=0.8)
    ax_out.annotate(
        f"Wanted\n{f_wanted_out_khz:.0f} kHz",
        xy=(f_wanted_out_khz, wanted_peak), xytext=(250, -20),
        color="#4ade80", fontsize=11, va="top",
        arrowprops=dict(arrowstyle="->", color="#4ade80", lw=1.2),
    )

    # Complex tone at f_jammer aliases to f_jammer mod fs_out,
    # wrapped to (-fs_out/2, fs_out/2].
    f_alias = f_jammer % fs_out
    if f_alias > fs_out / 2:
        f_alias -= fs_out
    f_alias_khz = f_alias / 1e3
    alias_peak = float(amp_out[int(np.argmin(np.abs(freq_out_khz - f_alias_khz)))])
    ax_out.axvline(f_alias_khz, color="#f87171",
                   lw=1.0, linestyle="-.", alpha=0.8)
    ax_out.annotate(
        f"Jammer alias\n{f_alias_khz:.0f} kHz\n(attenuated)",
        xy=(f_alias_khz, alias_peak), xytext=(-500, -45),
        color="#f87171", fontsize=11, va="top",
        arrowprops=dict(arrowstyle="->", color="#f87171", lw=1.2),
    )

    ax_out.set_xlim(-fs_in / 2 / 1e3, fs_in / 2 / 1e3)
    ax_out.set_ylim(-120, 10)
    ax_out.set_xlabel("Frequency (kHz)")
    ax_out.set_ylabel("Amplitude (dBFS)")
    ax_out.set_title("Output spectrum  (decimated)", loc="right", color="#f1f5f9")
    ax_out.grid(True, color="#374151", lw=0.4)
    ax_out.set_facecolor("#111827")
    ax_out.tick_params(colors="#d1d5db")
    for sp in ax_out.spines.values():
        sp.set_color("#374151")

    # Dark figure background to match the panel style
    fig.patch.set_facecolor("#0f172a")
    for ax in (ax_in, ax_q15, ax_out):
        ax.xaxis.label.set_color("#d1d5db")
        ax.yaxis.label.set_color("#d1d5db")
        ax.title.set_color("#f1f5f9")

    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"--- 5. Spectral plot saved → {out_path}")


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    print("=== doppler CIC decimation filter demo ===\n")
    demo_basic()
    demo_alias_rejection()
    demo_reconfigure()
    x, y_settled, fs_in, fs_out, R, f_wanted, f_jammer = demo_sdr_pipeline()
    demo_spectral_plot(
        x, y_settled,
        fs_in=fs_in, fs_out=fs_out, R=R,
        f_wanted=f_wanted, f_jammer=f_jammer,
    )
    print("Done.")
