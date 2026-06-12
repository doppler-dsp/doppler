"""wcdma_carriers_demo.py — 4 WCDMA carriers, measured with Welch + AccTrace.

A multi-carrier monitoring scene built entirely with doppler's own waveform
generator (``doppler.wfm``) and analysed with the new spectral-measurement
suite (``doppler.spectral.Welch`` and ``doppler.accumulator.AccTrace``).

The scene: four WCDMA-like downlink carriers — QPSK at the 3.84 Mcps chip rate,
one per 5 MHz channel — placed at -7.5, -2.5, +2.5 and +7.5 MHz over a single
AWGN floor, at deliberately different power levels (0, -3, -6, -10 dBFS). This
is the everyday "is every carrier at the right power?" question a spectrum
monitor answers.

Four panels:

  1. The averaged PSD (Welch, Kaiser window, linear/mean trace). Channel edges
     shaded; the measured per-channel power and the noise floor annotated.
  2. Trace averaging, shown with AccTrace directly: one raw periodogram (noisy)
     vs. the mean trace (variance collapses) vs. the max-hold envelope. Welch
     wraps exactly this — window -> FFT -> power -> AccTrace — so the panel is
     also a peek under Welch's hood.
  3. Per-channel band power: ``Welch.band_power(edges)`` vs. the nominal levels,
     with ``total_band_power`` for the whole occupied span.
  4. Per-channel measurements: occupied bandwidth (99 %), in-channel SNR, and
     adjacent-channel leakage (ACLR) derived from the band powers.

Run: ``uv run python examples/python/wcdma_carriers_demo.py``
"""

from __future__ import annotations

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from doppler.accumulator import AccTrace
from doppler.spectral import FFT, Welch, kaiser_window
from doppler.wfm import Composer, Segment, noise, qpsk

# ── scene parameters ────────────────────────────────────────────────────────
FS = 30.72e6  # 8 x 3.84 Mcps — a standard WCDMA capture rate
CHIP_RATE = 3.84e6  # WCDMA downlink chip rate
SPS = int(round(FS / CHIP_RATE))  # 8 samples/chip
RRC_BETA = 0.22  # WCDMA root-raised-cosine roll-off
RRC_SPAN = 8  # RRC support, ±8 symbols
CH_BW = 5.0e6  # nominal 5 MHz channel spacing
NFFT = 4096  # FFT / Welch frame size
N_FRAMES = 96  # frames to average
KAISER_BETA = 12.0  # ~ -90 dB sidelobes: resolves the -10 dB carrier

# Four carriers: (center frequency Hz, level dBFS).
CARRIERS = [
    (-7.5e6, 0.0),
    (-2.5e6, -3.0),
    (2.5e6, -6.0),
    (7.5e6, -10.0),
]
FLOOR_DBFS = -70.0  # AWGN floor, well below the weakest (-10 dBFS) carrier


def _rrc_carrier(fc: float, level: float, seed: int, n: int) -> np.ndarray:
    """One RRC-shaped WCDMA carrier at offset ``fc``, scaled to ``level`` dBFS.

    Straight from doppler's waveform generator: ``qpsk(pulse="rrc")`` band-limits
    the QPSK chips with a root-raised-cosine pulse (WCDMA roll-off 0.22) — the
    shaping the default sample-and-hold QPSK does not do — and ``freq=fc`` mixes
    the carrier to its channel centre, all in the C engine. The RRC taps are
    unit-transmit-power scaled, so the carrier is already unit power and the
    ``level`` factor lets ``band_power`` read the level directly.
    """
    sig = (
        qpsk(
            sps=SPS,
            pulse="rrc",
            rrc_beta=RRC_BETA,
            rrc_span=RRC_SPAN,
            freq=fc,
            fs=FS,
            seed=seed,
        )
        .steps(n)
        .astype(np.complex64)
    )
    return (sig * 10.0 ** (level / 20.0)).astype(np.complex64)


def build_scene() -> np.ndarray:
    """Build the 4-carrier scene from doppler's generator + RRC shaping.

    Four RRC-shaped QPSK carriers (see :func:`_rrc_carrier`) summed over a
    composed AWGN floor. The floor is produced by the same waveform generator
    (a ``noise`` synth at ``FLOOR_DBFS`` through ``Composer``) so signal and
    noise share the 0 dBFS = unit-power reference.
    """
    n = NFFT * N_FRAMES
    scene = np.zeros(n, dtype=np.complex64)
    for i, (fc, lvl) in enumerate(CARRIERS):
        scene += _rrc_carrier(fc, lvl, 10 + i, n)

    floor = Composer(
        Segment.sum(noise(level=FLOOR_DBFS), num_samples=n, fs=FS)
    ).compose()
    return (scene + floor[:n]).astype(np.complex64)


def channel_edges() -> np.ndarray:
    """Flat [lo0, hi0, lo1, hi1, ...] channel edges in Hz for band_power."""
    edges = []
    for fc, _ in CARRIERS:
        edges += [fc - CH_BW / 2, fc + CH_BW / 2]
    return np.array(edges, dtype=np.float64)


def main() -> None:
    x = build_scene()

    # ── Welch: averaged PSD + all measurements ──────────────────────────────
    w = Welch(n=NFFT, fs=FS, window="kaiser", beta=KAISER_BETA, mode="mean")
    w.accumulate(x)
    # psd_db() / band_power() return zero-copy views into Welch's internal
    # buffers, so np.array(...) snapshots them before a later call to the same
    # method (e.g. the ACLR band_power below) reuses that buffer.
    psd = np.array(w.psd_db())
    freqs = (np.arange(NFFT) - NFFT // 2) * (FS / NFFT)  # DC-centred bin freqs

    edges = channel_edges()
    band_db = np.array(w.band_power(edges))  # per-channel power, dB
    total_db = w.total_band_power(edges)  # whole occupied span, dB
    nf = w.noise_floor()  # median dB level

    # ── AccTrace directly: raw vs mean vs max-hold (panel 2) ────────────────
    win = np.empty(NFFT, dtype=np.float32)
    kaiser_window(win, KAISER_BETA)
    cg2 = float(win.sum()) ** 2  # coherent-gain normalisation (matches Welch)
    fft = FFT(NFFT, -1)
    acc_mean = AccTrace(n=NFFT, mode="mean")
    acc_max = AccTrace(n=NFFT, mode="maxhold")
    raw0 = None
    for i in range(N_FRAMES):
        frame = (x[i * NFFT : (i + 1) * NFFT] * win).astype(np.complex64)
        p = np.fft.fftshift(np.abs(fft.execute_cf32(frame)) ** 2).astype(
            np.float32
        )
        acc_mean.accumulate(p)
        acc_max.accumulate(p)
        if raw0 is None:
            raw0 = p

    def to_db(t):
        return 10.0 * np.log10(np.maximum(t / cg2, 1e-20))

    raw_db, mean_db, max_db = (
        to_db(raw0),
        to_db(acc_mean.value()),
        to_db(acc_max.value()),
    )

    # ── per-channel measurements (panel 4) ──────────────────────────────────
    rows = []
    for (fc, lvl), bdb in zip(CARRIERS, band_db):
        lo, hi = fc - CH_BW / 2, fc + CH_BW / 2
        rows.append(
            {
                "fc": fc,
                "nominal": lvl,
                "power": float(bdb),
                "snr": w.snr(lo, hi),
            }
        )
    # ACLR: the strongest (edge) carrier's in-channel power vs. the empty 5 MHz
    # guard channel just outside it — the real adjacent-channel-leakage figure.
    guard = np.array(
        [-15.0e6, -10.0e6]
    )  # empty channel below the -7.5 MHz carrier
    adj_db = float(w.band_power(guard)[0])
    aclr = float(band_db[0] - adj_db)
    fmhz = freqs / 1e6

    # ── plot ────────────────────────────────────────────────────────────────
    fig, ax = plt.subplots(2, 2, figsize=(12, 8))

    # (1) averaged PSD with channel shading
    a = ax[0, 0]
    a.plot(fmhz, psd, lw=0.8, color="#1f77b4")
    for (fc, lvl), bdb in zip(CARRIERS, band_db):
        lo, hi = (fc - CH_BW / 2) / 1e6, (fc + CH_BW / 2) / 1e6
        a.axvspan(lo, hi, color="#1f77b4", alpha=0.07)
        a.annotate(
            f"{bdb:.1f} dB",
            (fc / 1e6, bdb),
            ha="center",
            va="bottom",
            fontsize=8,
            color="#b8860b",
        )
    a.axhline(
        nf, color="crimson", ls="--", lw=0.8, label=f"noise floor {nf:.1f} dB"
    )
    a.set_title("Welch averaged PSD — 4 WCDMA carriers")
    a.set_xlabel("frequency (MHz)")
    a.set_ylabel("power (dB)")
    a.legend(loc="upper right", fontsize=8)
    a.grid(alpha=0.3)

    # (2) trace averaging via AccTrace
    a = ax[0, 1]
    a.plot(fmhz, raw_db, lw=0.5, color="#cccccc", label="single periodogram")
    a.plot(fmhz, max_db, lw=0.7, color="#2ca02c", label="AccTrace max-hold")
    a.plot(fmhz, mean_db, lw=0.9, color="#1f77b4", label="AccTrace mean")
    a.set_title("Trace averaging (AccTrace) — variance vs. max-hold")
    a.set_xlabel("frequency (MHz)")
    a.set_ylabel("power (dB)")
    a.legend(loc="upper right", fontsize=8)
    a.grid(alpha=0.3)

    # (3) per-channel band power, relative to the strongest carrier, vs nominal
    a = ax[1, 0]
    idx = np.arange(len(CARRIERS))
    nominal = np.array([lvl for _, lvl in CARRIERS])
    meas_rel = band_db - band_db.max()  # re strongest channel
    nom_rel = nominal - nominal.max()
    a.bar(idx - 0.2, nom_rel, 0.4, label="programmed level", color="#aec7e8")
    a.bar(
        idx + 0.2,
        meas_rel,
        0.4,
        label="band_power() (re. peak)",
        color="#1f77b4",
    )
    for i, m in enumerate(meas_rel):
        a.annotate(f"{m:.1f}", (i + 0.2, m), ha="center", va="top", fontsize=8)
    a.set_xticks(idx)
    a.set_xticklabels([f"{fc / 1e6:+.1f}\nMHz" for fc, _ in CARRIERS])
    a.set_title(f"Per-channel power, re. strongest  (total {total_db:.1f} dB)")
    a.set_ylabel("relative power (dB)")
    a.legend(loc="lower left", fontsize=8)
    a.grid(alpha=0.3, axis="y")

    # (4) measurements table
    a = ax[1, 1]
    a.axis("off")
    lines = [
        f"{'channel':>10}  {'nom':>5}  {'meas':>6}  {'SNR':>6}",
        "  " + "-" * 32,
    ]
    for r in rows:
        lines.append(
            f"{r['fc'] / 1e6:>+8.1f}M  {r['nominal']:>5.0f}  "
            f"{r['power']:>6.1f}  {r['snr']:>5.1f}"
        )
    lines += [
        "  " + "-" * 32,
        f"  total band power   {total_db:>6.1f} dB",
        f"  noise floor        {nf:>6.1f} dB",
        f"  occupied BW (99%)  {w.occupied_bw(0.99) / 1e6:>6.2f} MHz",
        f"  ACLR (ch0->ch1)    {aclr:>6.1f} dB",
        f"  Welch RBW          {w.rbw / 1e3:>6.1f} kHz",
        f"  frames averaged    {w.count:>6d}",
    ]
    a.text(
        0.02,
        0.98,
        "\n".join(lines),
        family="monospace",
        fontsize=9,
        va="top",
        transform=a.transAxes,
    )
    a.set_title("Measurements")

    fig.suptitle(
        "Four WCDMA carriers at 0 / -3 / -6 / -10 dBFS — "
        "doppler.wfm + Welch / AccTrace",
        fontsize=12,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.96])
    fig.savefig("wcdma_carriers_demo.png", dpi=110)
    print("wrote wcdma_carriers_demo.png")
    print(
        f"  per-channel band power (dB): {[round(float(b), 1) for b in band_db]}"
    )
    print(
        f"  total {total_db:.1f} dB | noise floor {nf:.1f} dB | ACLR {aclr:.1f} dB"
    )


if __name__ == "__main__":
    main()
