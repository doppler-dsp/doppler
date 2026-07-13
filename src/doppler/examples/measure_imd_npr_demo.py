"""measure_imd_npr_demo.py — two-tone IMD/TOI and notched-noise NPR.

The companion to ``measure_demo.py`` (single-tone ADC metrics): the other two
analysers in ``doppler.measure``, driven on synthesised captures.

  (a) Two-tone IMD spectrum — two equal tones through a weak polynomial
      nonlinearity; ``IMDMeasure`` finds the fundamentals and the folded IM2
      (f2-f1) and IM3 (2f1-f2, 2f2-f1) products, annotated with IMD3/IMD2 and
      the third-order intercept (TOI).
  (b) Third-order intercept extrapolation — sweeping drive level, the
      fundamental rises 1:1 and IM3 rises 3:1; the lines meet at the **TOI**,
      the canonical figure of merit ``IMDMeasure`` reports directly.
  (c) Notched-noise NPR spectrum — band-limited noise with a carved notch is
      quantised by an ADC; ``NPRMeasure`` averages the in-band PSD and the
      noise that distortion + quantisation has dumped into the notch. NPR is
      their ratio.
  (d) NPR vs RMS loading, against the ideal-quantiser curve (Gray-Zeoli / ADI
      MT-005): quantisation-limited at low loading (NPR climbs 6 dB/octave),
      clipping-limited past the knee (NPR falls); the measured curve tracks the
      ideal and peaks at the optimal loading (~-13 dBFS RMS, ~52 dB for 10
      bits). NPR loading is RMS-to-full-scale by convention.

Every measurement feeds an `M = NAVG * N` capture, so `analyze()` averages
`NAVG = 8` segments (Welch's method) — a smoother spectrum and a lower-variance
notch/floor estimate than a single periodogram, at the same resolution
bandwidth.

Run:
    python examples/python/measure_imd_npr_demo.py
"""

from __future__ import annotations

import math

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from doppler.cvt import ADC
from doppler.measure import IMDMeasure, NPRMeasure
from doppler.source import AWGN, LO
from doppler.spectral import FFT

FS = 100e6  # 100 MHz sample rate
N = 1 << 14  # 16384-sample segment (sets the resolution bandwidth)
NAVG = 8  # segments averaged per measurement (Welch's method)
M = NAVG * N  # total capture length fed to analyze()
# matplotlib standard palette (tab10) by role
ACCENT, FUND, IM3, IM2, FLOOR = "C0", "C2", "C3", "C1", "C7"


A2, A3 = 0.02, 0.05  # weak-nonlinearity coefficients (the DUT model)


def two_tone(f1, f2, amp, a2=A2, a3=A3):
    """Two doppler-NCO (`source.LO`) tones through a weak memoryless
    nonlinearity y = x + a2 x^2 + a3 x^3 — the DUT model that creates the
    controlled IM2/IM3 products.  The spectrum backdrop the demo plots comes
    from the analyzer's own `spectrum_dbfs`, not a hand-rolled periodogram.
    The capture spans `M = NAVG * N` so `analyze()` averages `NAVG` segments.
    """
    x = amp * (LO(f1 / FS).steps(M).real + LO(f2 / FS).steps(M).real)
    return (x + a2 * x**2 + a3 * x**3).astype(np.float32)


def notched_noise(rms, active_lo, active_hi, notch_lo, notch_hi, seed=0):
    """Band-limited noise with a carved notch, scaled to an **RMS** level
    `rms` relative to full scale (= 1.0) — the NPR test stimulus.

    doppler-native: white noise from `source.AWGN`, shaped in the frequency
    domain with `spectral.FFT` (forward + inverse).  The band/notch geometry is
    the device-under-test model.  NPR loading is RMS-to-full-scale by
    convention — the Gaussian peak is ~12-13 dB above the RMS (the crest
    factor), so clipping sets in well before the RMS reaches 0 dBFS."""
    k = np.arange(M)
    freqs = np.abs(np.where(k < M // 2, k, k - M)) * (FS / M)  # |Hz| per bin
    keep = (
        (freqs >= active_lo)
        & (freqs <= active_hi)
        & ~((freqs >= notch_lo) & (freqs <= notch_hi))
    )
    white = AWGN(seed, 1.0).generate(M)  # M = NAVG*N complex white samples
    spec = FFT(M, -1).execute_cf32(white) * keep  # FFT, mask band + notch
    x = (FFT(M, 1).execute_cf32(spec.astype(np.complex64)) / M).real
    return (x / np.sqrt(np.mean(x**2)) * rms).astype(np.float32)


def npr_theory_db(rms_dbfs, bits, b_active, b_nyq):
    """Ideal-quantiser NPR vs RMS loading (Gray–Zeoli / ADI MT-005).

    The notch noise of an ideal N-bit converter under Gaussian loading is the
    sum of **granular** quantisation noise (q²/12, flat in dBFS so NPR climbs
    6 dB/octave with loading) and **overload** (clipping) noise of a Gaussian
    hard-limited at ±full-scale (negligible until the tails reach the rails,
    then rising fast). NPR is the in-band-to-notch power ratio; with the signal
    confined to ``b_active`` of the ``b_nyq`` Nyquist span while the converter
    noise is white across the whole span, the per-Hz ratio carries the
    ``b_nyq / b_active`` band-spreading term."""
    rms = 10 ** (rms_dbfs / 20)  # RMS / full-scale (V = 1)
    q = 2.0 ** (1 - bits)  # quantiser step (1 code) in full-scale units
    gamma = 1.0 / rms  # loading factor = full-scale / RMS
    q_up = 0.5 * math.erfc(gamma / math.sqrt(2.0))  # Gaussian upper tail
    phi = math.exp(-(gamma**2) / 2.0) / math.sqrt(2.0 * math.pi)
    p_clip = 2.0 * rms**2 * ((1.0 + gamma**2) * q_up - gamma * phi)
    p_quant = q**2 / 12.0
    return 10.0 * math.log10(rms**2 / (p_quant + p_clip)) + 10.0 * math.log10(
        b_nyq / b_active
    )


def main() -> None:
    fig, axes = plt.subplots(2, 2, figsize=(13, 9))

    # ── (a) two-tone IMD spectrum ─────────────────────────────────────────
    ax = axes[0, 0]
    f1, f2 = 9.013e6, 9.637e6
    amp0 = 0.35  # per-tone drive for panels (a) and (b)
    y = two_tone(f1, f2, amp=amp0)
    imd = IMDMeasure(n=N, fs=FS, dynamic_range_db=90.0)
    r = imd.analyze(y)
    # self-validation: the weak nonlinearity predicts the products in
    # closed form — per-tone IM3 amplitude (3/4)·a3·A³, IM2 amplitude
    # a2·A², and TOI = P1 - IMD3/2. The analyzer must recover them all,
    # at the right (folded) frequencies.
    imd3_th = 20 * math.log10(0.75 * A3 * amp0**2)
    imd2_th = 20 * math.log10(A2 * amp0)
    toi_th = 20 * math.log10(amp0) - imd3_th / 2
    print(
        f"(a) IMD3 {r.imd3_dbc:.1f} dBc (theory {imd3_th:.1f}), "
        f"IMD2 {r.imd2_dbc:.1f} dBc (theory {imd2_th:.1f}), "
        f"TOI {r.toi_dbfs:+.1f} dBFS (theory {toi_th:+.1f})"
    )
    assert abs(r.imd3_dbc - imd3_th) < 2.0, "IMD3 misses injected level"
    assert abs(r.imd2_dbc - imd2_th) < 2.0, "IMD2 misses injected level"
    assert abs(r.toi_dbfs - toi_th) < 2.0, "TOI misses the a3 prediction"
    rbw = FS / imd.nfft
    assert abs(r.imd3_lo_freq - (2 * f1 - f2)) < 3 * rbw, "IM3 mislocated"
    assert abs(r.imd2_freq - (f2 - f1)) < 3 * rbw, "IM2 mislocated"
    db = imd.spectrum_dbfs(y)  # the same averaged PSD the metrics use
    half = imd.nfft // 2
    freqs = np.arange(half) * FS / imd.nfft / 1e6  # MHz (one-sided)
    ax.plot(freqs, db[half:], color=ACCENT, lw=0.7)
    for f, c, lbl in (
        (r.f1, FUND, "fundamentals"),
        (r.f2, FUND, None),
        (r.imd3_lo_freq, IM3, "IM3 (2f₁−f₂, 2f₂−f₁)"),
        (r.imd3_hi_freq, IM3, None),
        (r.imd2_freq, IM2, "IM2 (f₂−f₁)"),
    ):
        ax.axvline(f / 1e6, color=c, ls="--", lw=1.0, alpha=0.8, label=lbl)
    box = (
        f"f₁,f₂   {r.f1 / 1e6:.2f}, {r.f2 / 1e6:.2f} MHz\n"
        f"IMD3   {r.imd3_dbc:.1f} dBc\n"
        f"IMD2   {r.imd2_dbc:.1f} dBc\n"
        f"TOI    {r.toi_dbfs:+.1f} dBFS"
    )
    ax.text(
        0.97,
        0.95,
        box,
        transform=ax.transAxes,
        ha="right",
        va="top",
        family="monospace",
        fontsize=8,
        bbox={"boxstyle": "round", "fc": "#f8fafc", "ec": "#cbd5e1"},
    )
    ax.set(
        title="(a) two-tone IMD — fundamentals + folded IM products",
        xlabel="frequency (MHz)",
        ylabel="dBFS",
        xlim=(0, 25),
        ylim=(-130, 5),
    )
    ax.legend(loc="lower right", fontsize=7)

    # ── (b) third-order intercept extrapolation ───────────────────────────
    ax = axes[0, 1]
    drives = np.arange(-40, -2, 3.0)  # dB relative to the panel-(a) drive
    p_fund, p_im3 = [], []
    for d in drives:
        amp = amp0 * 10 ** (d / 20)
        rd = imd.analyze(two_tone(f1, f2, amp=amp))
        p_fund.append(rd.p1_dbfs)
        p_im3.append(rd.p1_dbfs + rd.imd3_dbc)  # IM3 absolute level (dBFS)
    p_fund = np.array(p_fund)
    p_im3 = np.array(p_im3)
    ax.plot(drives, p_fund, "o", color=FUND, ms=4, label="fundamental (1:1)")
    ax.plot(drives, p_im3, "s", color=IM3, ms=4, label="IM3 (3:1)")
    # Fit + extrapolate to the intercept, using the clean middle drives:
    # below ~-32 dB the IM3 product falls under the analysis floor (the
    # measured points flatten), and the very top drives begin to
    # compress — either would bias the 3:1 slope.
    lo = (drives >= -31) & (drives <= -13)
    cf = np.polyfit(drives[lo], p_fund[lo], 1)
    ci = np.polyfit(drives[lo], p_im3[lo], 1)
    xe = np.array([drives[0], (ci[1] - cf[1]) / (cf[0] - ci[0])])
    ax.plot(xe, np.polyval(cf, xe), "-", color=FUND, lw=1, alpha=0.6)
    ax.plot(xe, np.polyval(ci, xe), "-", color=IM3, lw=1, alpha=0.6)
    toi = float(np.polyval(cf, xe[1]))
    # self-validation: canonical intercept behaviour — the fundamental
    # rises 1:1, IM3 rises 3:1, and the extrapolated intersection lands
    # on the TOI the analyzer reported directly in panel (a).
    print(
        f"(b) slopes: fund {cf[0]:.2f} (1:1), IM3 {ci[0]:.2f} (3:1); "
        f"extrapolated TOI {toi:+.1f} dBFS vs reported {r.toi_dbfs:+.1f}"
    )
    assert abs(cf[0] - 1.0) < 0.1, "fundamental slope is not 1:1"
    assert abs(ci[0] - 3.0) < 0.2, "IM3 slope is not 3:1"
    assert abs(toi - r.toi_dbfs) < 1.5, "extrapolated TOI misses reported"
    ax.plot(xe[1], toi, "*", color="C4", ms=16, label="TOI")
    ax.annotate(
        f"TOI ≈ {r.toi_dbfs:+.1f} dBFS",
        xy=(xe[1], toi),
        xytext=(-8, -12),
        textcoords="offset points",
        fontsize=8,
        color="C4",
        ha="right",
    )
    ax.set(
        title="(b) third-order intercept (1:1 vs 3:1 slopes)",
        xlabel="two-tone drive (dB, rel. panel a)",
        ylabel="output level (dBFS)",
    )
    ax.legend(fontsize=8, loc="upper left")
    ax.grid(alpha=0.3)

    # ── (c) notched-noise NPR spectrum ────────────────────────────────────
    ax = axes[1, 0]
    # Broadband (≈full-Nyquist) loading with a centred notch — the canonical
    # NPR test, so the measured ratio matches the MT-005 ideal directly.
    alo, ahi, nlo, nhi = 1e6, 49e6, 24e6, 26e6
    guard = 0.5e6
    load_dbfs = -12.4  # RMS loading near the 10-bit optimum (MT-005)
    codes = (
        ADC(10, 0.0, 0)
        .steps(notched_noise(10 ** (load_dbfs / 20), alo, ahi, nlo, nhi))
        .astype(np.float32)
    )
    npr = NPRMeasure(n=N, fs=FS, bits=10)  # bits sets the dBFS reference
    g = npr.analyze(codes, alo, ahi, nlo, nhi, guard)
    # self-validation: at near-optimal loading the measured NPR must sit
    # on the MT-005 ideal-quantiser value for 10 bits.
    npr_th = npr_theory_db(load_dbfs, 10, (ahi - alo) - (nhi - nlo), FS / 2.0)
    print(f"(c) NPR {g.npr_db:.1f} dB vs ideal 10-bit {npr_th:.1f} dB")
    assert abs(g.npr_db - npr_th) < 2.0, "NPR misses the MT-005 ideal"
    db = npr.spectrum_dbfs(codes)  # the same averaged PSD the metrics use
    half = npr.nfft // 2
    freqs = np.arange(half) * FS / npr.nfft / 1e6  # MHz (one-sided)
    ax.plot(freqs, db[half:], color=ACCENT, lw=0.5)
    ax.axvspan(
        alo / 1e6, ahi / 1e6, color=FUND, alpha=0.06, label="active band"
    )
    ax.axvspan(nlo / 1e6, nhi / 1e6, color=IM3, alpha=0.12, label="notch")
    ax.hlines(
        g.inband_psd_dbfs,
        alo / 1e6,
        ahi / 1e6,
        color=FUND,
        lw=1.4,
        label=f"in-band PSD {g.inband_psd_dbfs:.0f} dBFS",
    )
    ax.hlines(
        g.notch_psd_dbfs,
        nlo / 1e6,
        nhi / 1e6,
        color=IM3,
        lw=1.4,
        label=f"notch PSD {g.notch_psd_dbfs:.0f} dBFS",
    )
    ax.annotate(
        "",
        xy=((nlo + nhi) / 2e6, g.notch_psd_dbfs),
        xytext=((nlo + nhi) / 2e6, g.inband_psd_dbfs),
        arrowprops={"arrowstyle": "<->", "color": "C4", "lw": 1.4},
    )
    ax.text(
        (nlo + nhi) / 2e6 + 1.0,
        (g.inband_psd_dbfs + g.notch_psd_dbfs) / 2,
        f"NPR\n{g.npr_db:.1f} dB",
        color="C4",
        fontsize=8,
        va="center",
    )
    ax.set(
        title=f"(c) notched-noise NPR — 10-bit ADC @ {load_dbfs:.0f} dBFS RMS",
        xlabel="frequency (MHz)",
        ylabel="dBFS",
        xlim=(0, 50),
        ylim=(-110, 0),
    )
    ax.legend(loc="lower right", fontsize=7)

    # ── (d) NPR vs loading: measured vs the MT-005 ideal ──────────────────
    ax = axes[1, 1]
    b_active = (ahi - alo) - (nhi - nlo)  # signal-occupied bandwidth
    b_nyq = FS / 2.0
    loadings = np.arange(-30.0, 0.1, 1.5)  # RMS loading, dBFS
    nprs = []
    for ld in loadings:
        codes = (
            ADC(10, 0.0, 0)
            .steps(notched_noise(10 ** (ld / 20), alo, ahi, nlo, nhi, seed=1))
            .astype(np.float32)
        )
        nprs.append(npr.analyze(codes, alo, ahi, nlo, nhi, guard).npr_db)
    nprs = np.array(nprs)
    theory = np.array(
        [npr_theory_db(ld, 10, b_active, b_nyq) for ld in loadings]
    )
    kpeak = int(np.argmax(nprs))
    # self-validation: the measured curve tracks the ideal-quantiser
    # theory across the whole loading sweep (quantisation-limited rise,
    # clipping-limited fall) and peaks at the MT-005 sweet spot ~-13
    # dBFS for 10 bits.
    dev = float(np.max(np.abs(nprs - theory)))
    print(
        f"(d) max |measured - ideal| {dev:.2f} dB, peak "
        f"{nprs[kpeak]:.1f} dB @ {loadings[kpeak]:.1f} dBFS RMS"
    )
    assert dev < 2.5, "NPR departs from the MT-005 ideal curve"
    assert abs(loadings[kpeak] + 13.0) <= 3.0, "NPR sweet spot mislocated"
    ax.plot(
        loadings,
        theory,
        "-",
        color=FLOOR,
        lw=1.6,
        label="ideal 10-bit (MT-005)",
    )
    ax.plot(loadings, nprs, "o", color=ACCENT, ms=4, label="measured")
    ax.plot(
        loadings[kpeak],
        nprs[kpeak],
        "*",
        color="C4",
        ms=16,
        label=f"sweet spot {nprs[kpeak]:.0f} dB @ {loadings[kpeak]:.0f} dBFS",
    )
    ax.axvline(loadings[kpeak], color="C4", ls=":", lw=1)
    ax.set(
        title="(d) NPR vs loading — quantisation floor vs clipping",
        xlabel="noise loading (dBFS RMS)",
        ylabel="NPR (dB)",
    )
    ax.legend(fontsize=8, loc="lower center")
    ax.grid(alpha=0.3)

    fig.suptitle(
        "doppler.measure — two-tone IMD/TOI and notched-noise NPR",
        fontsize=14,
        weight="bold",
    )
    fig.tight_layout(rect=(0, 0, 1, 0.98))
    fig.savefig("measure_imd_npr_demo.png", dpi=110)
    print("wrote measure_imd_npr_demo.png")


if __name__ == "__main__":
    main()
