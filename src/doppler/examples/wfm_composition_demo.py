"""wfm_composition_demo.py — composing a scene with .sum() and .add().

An end-to-end tour of the waveform composer's amplitude + composition model
(doppler 0.11): per-source ``level``, multi-source ``Segment.sum`` over one
resolved noise floor, the ``Segment.add`` timeline, and clip detection +
``headroom`` at the writer. Four panels:

Top-left — **the scene's spectrum.** ``Segment.sum`` mixes a QPSK signal of
interest (−10 dBFS) with a full-scale CW interferer at +200 kHz over one shared
noise floor. SNR lives on the SoI (``snr=15`` Es/No); the composer resolves
that
into an explicit AWGN floor in C, so the SoI sits its **over-fs SNR** above the
floor here (the Es/No is realised later, by the receiver's matched filter).

Top-right — **the timeline.** ``Segment.add`` sequences a clean preamble tone
burst, then the scene — a spectrogram shows the time-frequency structure.

Bottom-left — **PAPR and headroom.** The sum is no longer constant-envelope, so
its peak runs past full-scale and an integer capture clips (red). ``headroom``
backs the whole composite off by a few dB so the peak fits — one scale, so
every
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

# --8<-- [start:scene]
import numpy as np

from doppler.wfm import Composer, Segment, Writer, qpsk, tone

FS = 1e6  # scene sample rate, Hz

# 1. Mix a scene: a QPSK SoI under a CW interferer, over one noise
#    floor. fs on the sum resolves every absolute freq below — without
#    it the default fs=1.0 silently aliases the "+200 kHz" tone to DC.
soi = qpsk(snr=15, snr_mode="esno", sps=8, level=-10.0, seed=1)
inter = tone(freq=2e5, level=-3.0)  # −3 dBFS CW at +200 kHz
scene = Segment.sum(soi, inter, num_samples=1 << 16, fs=FS)

# 2. Sequence it after a preamble (time, not frequency).
preamble = Segment(
    "tone", freq=-3e5, fs=FS, num_samples=16384, off_samples=8192
)
timeline = preamble.add(scene)

x = Composer(timeline).compose()  # → complex64
# --8<-- [end:scene]

# The narrative block above must stay self-contained (the gallery page
# includes it verbatim), so this import follows it. noqa: E402 — import
# not at top of file, by design.
from doppler.spectral import PSD  # noqa: E402

N = 1 << 16  # == the scene's num_samples above


def psd_db(x, nfft=1024):
    """Averaged PSD (dB, DC-centred), normalised to its own peak.

    Thin call to the C-backed ``doppler.spectral.PSD`` — Welch averaging with
    a Hann window, the same estimator the analyzer/measure suite is built on.
    """
    est = PSD(n=nfft, fs=FS, window="hann", mode="mean")
    est.accumulate(np.asarray(x, dtype=np.complex64))
    p = est.psd_db()
    return p - p.max()


# A short-time spectrogram has no single-shot PSD analogue, so this STFT helper
# stays explicit (per-frame FFT magnitude, not an averaged power estimate).
def spectrogram_db(x, nfft=256, hop=64):
    win = np.hanning(nfft)
    cols = [
        np.fft.fftshift(np.abs(np.fft.fft(x[i : i + nfft] * win)))
        for i in range(0, len(x) - nfft, hop)
    ]
    s = 20 * np.log10(np.array(cols).T + 1e-9)
    return s - s.max()


# The panels look at the scene and the timeline separately: `x` above is
# the composed timeline; re-compose the scene alone for the scene-only
# panels (compose is deterministic — every seed lives in the spec).
xs = Composer([scene]).compose().astype(np.complex128)

# ── PAPR / clipping: the raw peak vs a headroom-backed version ──────────────
peak = float(np.max(np.abs(np.concatenate([xs.real, xs.imag]))))
peak_dbfs = 20 * np.log10(peak)
headroom_db = float(np.ceil(peak_dbfs))  # just enough to fit ±1.0
xh = xs * 10 ** (-headroom_db / 20)
clip_frac = float(np.mean((np.abs(xs.real) > 1) | (np.abs(xs.imag) > 1)))

# ── the writer does the same bookkeeping for free ────────────────────────────
# --8<-- [start:writer]
with Writer("scene.cf32", sample_type="ci16", headroom=4.0) as w:
    w.write(x)
    print(f"peak {w.peak_dbfs:+.1f} dBFS, clipped: {w.clipped}")

# Or detect first, then dial in exactly enough headroom:
with Writer("probe.ci16", sample_type="ci16") as w:
    w.track_clipping()
    w.write(x)
    need = max(0.0, np.ceil(w.peak_dbfs))  # dB to fit under full scale
# --8<-- [end:writer]
# The probe writer's suggested headroom is exactly what the PAPR panel
# computed by hand from the raw samples.
assert need == headroom_db, f"writer wants {need} dB, panel {headroom_db}"

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
scene_db = psd_db(xs)
ax[0, 0].plot(f, scene_db, lw=0.8, color="#1f77b4")
ax[0, 0].set_title("Scene spectrum — .sum() of SoI + interferer + floor")
ax[0, 0].set_xlabel("frequency (kHz)")
ax[0, 0].set_ylabel("dB (rel. peak)")
ax[0, 0].annotate(
    "CW interferer\n−3 dBFS",
    xy=(200, -2),
    xytext=(60, -14),
    fontsize=9,
    arrowprops={"arrowstyle": "->", "color": "k"},
)
ax[0, 0].annotate(
    "QPSK SoI\n−10 dBFS",
    xy=(0, -26),
    xytext=(-230, -16),
    fontsize=9,
    arrowprops={"arrowstyle": "->", "color": "k"},
)
ax[0, 0].annotate(
    "resolved noise floor",
    xy=(330, -52),
    xytext=(120, -64),
    fontsize=9,
    arrowprops={"arrowstyle": "->", "color": "k"},
)
ax[0, 0].set_ylim(-80, 5)
ax[0, 0].grid(alpha=0.3)

# B: timeline spectrogram
s = spectrogram_db(x)
ax[0, 1].imshow(
    s,
    aspect="auto",
    origin="lower",
    extent=[0, len(x) / FS * 1e3, -FS / 2e3, FS / 2e3],
    cmap="magma",
    vmin=-70,
    vmax=0,
)
ax[0, 1].set_title("Timeline — .add() sequences preamble → gap → scene")
ax[0, 1].set_xlabel("time (ms)")
ax[0, 1].set_ylabel("frequency (kHz)")

# C: PAPR / headroom
t = np.arange(2000) / FS * 1e6  # µs
mag = np.abs(xs[:2000])
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

# ── validate ─────────────────────────────────────────────────────────────────
# Scene power: −3 dBFS tone + −10 dBFS QPSK + the floor the composer
# resolved from snr=15 Es/No at 8 sps (per-sample SNR is 9 dB lower) —
# .sum()'s amplitude model is additive in linear power.
snr_fs = 15.0 - 10.0 * np.log10(8.0)
exp_db = 10.0 * np.log10(
    10.0 ** (-3.0 / 10.0) + 0.1 + 0.1 * 10.0 ** (-snr_fs / 10.0)
)
x_db = 10.0 * np.log10(float(np.mean(np.abs(xs) ** 2)))
assert abs(x_db - exp_db) < 0.5, f"scene {x_db:.2f} dB != {exp_db:.2f} dB"
# The −3 dBFS CW interferer is the spectral peak, at its programmed
# +200 kHz (within a couple of 1024-point PSD bins, ~1 kHz each).
f_pk = float(f[int(np.argmax(scene_db))]) * 1e3  # Hz
assert abs(f_pk - 2.0e5) < 2.5e3, f"CW peak at {f_pk / 1e3:.1f} kHz"
# The .add() timeline is sample-accurate: preamble off (N/8) + on (N/4),
# then the N-sample scene.
assert len(x) == N // 8 + N // 4 + N, f"timeline length {len(x)}"
# Headroom: the raw composite really clips an integer capture, and the
# backed-off copy fits inside ±1.0 full scale with the SNR untouched.
assert clip_frac > 0.001, "raw composite unexpectedly fits full scale"
xh_peak = float(np.max(np.abs(np.concatenate([xh.real, xh.imag]))))
assert xh_peak <= 1.0, f"headroom copy still peaks at {xh_peak:.3f}"
# The matched-filtered SoI realises its programmed 15 dB Es/No: measure it
# decision-directed from the constellation cloud (scale fitted, then EVM).
dec = (np.sign(syms.real) + 1j * np.sign(syms.imag)) / np.sqrt(2)
scale = float(np.mean(np.abs(syms)) / np.mean(np.abs(dec)))
evm2 = float(np.mean(np.abs(syms - scale * dec) ** 2))
esno_meas = 10.0 * np.log10(scale**2 / evm2)
assert abs(esno_meas - 15.0) < 1.0, f"SoI Es/No {esno_meas:.2f} dB != 15"
print(
    f"validated: scene {x_db:+.2f} dBFS (exp {exp_db:+.2f}), CW at "
    f"{f_pk / 1e3:.0f} kHz,\n  timeline {len(x)} samples, headroom fits, "
    f"SoI Es/No {esno_meas:.1f} dB"
)
