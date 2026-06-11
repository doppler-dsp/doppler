"""wfm_composition_demo.py — composing a scene with .sum() and .add().

An end-to-end tour of the waveform composer's amplitude + composition model
(doppler 0.11): per-source ``level``, multi-source ``Segment.sum`` over one
resolved noise floor, the ``Segment.add`` timeline, and clip detection +
``headroom`` at the writer. Four panels:

Top-left — **the scene's spectrum.** ``Segment.sum`` mixes a QPSK signal of
interest (−10 dBFS) with a full-scale CW interferer at +200 kHz over one shared
noise floor. SNR lives on the SoI (``snr=15`` Es/No); the composer resolves that
into an explicit AWGN floor in C, so the SoI sits its **over-fs SNR** above the
floor here (the Es/No is realised later, by the receiver's matched filter).

Top-right — **the timeline.** ``Segment.add`` sequences a clean preamble tone
burst, then the scene — a spectrogram shows the time-frequency structure.

Bottom-left — **PAPR and headroom.** The sum is no longer constant-envelope, so
its peak runs past full-scale and an integer capture clips (red). ``headroom``
backs the whole composite off by a few dB so the peak fits — one scale, so every
power ratio (and the SNR) is unchanged.

Bottom-right — **SNR on the source.** The SoI in isolation, matched-filtered:
the four Gray-coded QPSK points and their cloud are the ``snr=15`` Es/No made
visible — the SNR that the scene's floor was resolved from.

Run:
    python examples/python/wfm_composition_demo.py
"""

from __future__ import annotations

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from doppler.wfm import (
    Composer,
    Segment,
    qpsk,
    tone,
)

FS = 1e6
N = 1 << 16


def welch_db(x, nfft=1024):
    """Averaged periodogram (dB, fftshifted), normalised to its own peak."""
    win = np.hanning(nfft)
    acc, nseg = np.zeros(nfft), 0
    for i in range(0, len(x) - nfft, nfft // 2):
        seg = x[i : i + nfft] * win
        acc += np.abs(np.fft.fftshift(np.fft.fft(seg))) ** 2
        nseg += 1
    p = 10 * np.log10(acc / nseg + 1e-12)
    return p - p.max()


def spectrogram_db(x, nfft=256, hop=64):
    win = np.hanning(nfft)
    cols = [
        np.fft.fftshift(np.abs(np.fft.fft(x[i : i + nfft] * win)))
        for i in range(0, len(x) - nfft, hop)
    ]
    s = 20 * np.log10(np.array(cols).T + 1e-9)
    return s - s.max()


# ── the scene: a QPSK SoI under a full-scale CW interferer, one floor ────────
soi = qpsk(snr=15, snr_mode="esno", sps=8, level=-10.0, seed=1)
interferer = tone(freq=2.0e5, level=-3.0)  # −3 dBFS CW at +200 kHz
scene = Segment.sum(soi, interferer, num_samples=N)
x = Composer([scene]).compose().astype(np.complex128)

# ── the timeline: a preamble tone burst, then the scene ─────────────────────
preamble = Segment("tone", freq=-3.0e5, num_samples=N // 4, off_samples=N // 8)
timeline = preamble.add(scene)
xt = Composer(timeline).compose()

# ── PAPR / clipping: the raw peak vs a headroom-backed version ──────────────
peak = float(np.max(np.abs(np.concatenate([x.real, x.imag]))))
peak_dbfs = 20 * np.log10(peak)
headroom_db = float(np.ceil(peak_dbfs))  # just enough to fit ±1.0
xh = x * 10 ** (-headroom_db / 20)
clip_frac = float(np.mean((np.abs(x.real) > 1) | (np.abs(x.imag) > 1)))

# ── the SoI in isolation, matched-filtered → its Es/No constellation ────────
SPS = 8
soi_only = Composer(
    type="qpsk",
    snr=15.0,
    snr_mode="esno",
    sps=SPS,
    seed=1,
    num_samples=SPS * 800,
).compose()
syms = soi_only.reshape(-1, SPS).mean(axis=1)  # boxcar matched filter

# ── plot ────────────────────────────────────────────────────────────────────
fig, ax = plt.subplots(2, 2, figsize=(12, 9))
fig.suptitle(
    "Composing a scene — Segment.sum (mix) + Segment.add (sequence)",
    fontsize=14,
    fontweight="bold",
)

# A: scene spectrum
f = np.linspace(-0.5, 0.5, 1024) * FS / 1e3  # kHz
ax[0, 0].plot(f, welch_db(x), lw=0.8, color="#1f77b4")
ax[0, 0].set_title("Scene spectrum — .sum() of SoI + interferer + floor")
ax[0, 0].set_xlabel("frequency (kHz)")
ax[0, 0].set_ylabel("dB (rel. peak)")
ax[0, 0].annotate(
    "CW interferer\n−3 dBFS",
    xy=(200, -2),
    xytext=(60, -14),
    fontsize=9,
    arrowprops=dict(arrowstyle="->", color="k"),
)
ax[0, 0].annotate(
    "QPSK SoI\n−10 dBFS",
    xy=(0, -26),
    xytext=(-230, -16),
    fontsize=9,
    arrowprops=dict(arrowstyle="->", color="k"),
)
ax[0, 0].annotate(
    "resolved noise floor",
    xy=(330, -52),
    xytext=(120, -64),
    fontsize=9,
    arrowprops=dict(arrowstyle="->", color="k"),
)
ax[0, 0].set_ylim(-80, 5)
ax[0, 0].grid(alpha=0.3)

# B: timeline spectrogram
s = spectrogram_db(xt)
ax[0, 1].imshow(
    s,
    aspect="auto",
    origin="lower",
    extent=[0, len(xt) / FS * 1e3, -FS / 2e3, FS / 2e3],
    cmap="magma",
    vmin=-70,
    vmax=0,
)
ax[0, 1].set_title("Timeline — .add() sequences preamble → gap → scene")
ax[0, 1].set_xlabel("time (ms)")
ax[0, 1].set_ylabel("frequency (kHz)")

# C: PAPR / headroom
t = np.arange(2000) / FS * 1e6  # µs
mag = np.abs(x[:2000])
magh = np.abs(xh[:2000])
ax[1, 0].axhline(1.0, color="k", lw=1.0, ls="--", label="full scale ±1.0")
ax[1, 0].plot(
    t, mag, lw=0.6, color="#d62728", label=f"raw (peak {peak_dbfs:+.1f} dBFS)"
)
ax[1, 0].plot(
    t, magh, lw=0.6, color="#2ca02c", label=f"−{headroom_db:.0f} dB headroom"
)
ax[1, 0].fill_between(t, 1.0, mag, where=mag > 1.0, color="#d62728", alpha=0.3)
ax[1, 0].set_title(
    f"PAPR & headroom — raw clips {clip_frac * 100:.1f}% of samples"
)
ax[1, 0].set_xlabel("time (µs)")
ax[1, 0].set_ylabel("|x|")
ax[1, 0].legend(loc="upper right", fontsize=8)
ax[1, 0].grid(alpha=0.3)

# D: SoI constellation (Es/No on the source)
ax[1, 1].scatter(syms.real, syms.imag, s=4, alpha=0.3, color="#1f77b4")
ax[1, 1].set_title("SNR on the source — SoI matched-filtered (15 dB Es/No)")
ax[1, 1].set_xlabel("I")
ax[1, 1].set_ylabel("Q")
ax[1, 1].axhline(0, color="k", lw=0.5)
ax[1, 1].axvline(0, color="k", lw=0.5)
ax[1, 1].set_aspect("equal")
ax[1, 1].grid(alpha=0.3)

fig.tight_layout(rect=[0, 0, 1, 0.97])
fig.savefig("wfm_composition_demo.png", dpi=110)
print(
    f"peak {peak_dbfs:+.1f} dBFS, {clip_frac * 100:.1f}% clipped, "
    f"headroom {headroom_db:.0f} dB → wfm_composition_demo.png"
)
