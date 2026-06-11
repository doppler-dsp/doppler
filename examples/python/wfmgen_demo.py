"""wfmgen_demo.py — the wfmgen waveform engine: four waveforms, four views.

A single declarative engine (`doppler.wfm.Synth`) produces every waveform
type the `wfmgen` tool emits. This demo drives it directly from
Python and shows each type through the view that makes its structure obvious:

Top-left — **tone** at `fn = 0.10` (relative to `fs`) at 30 dB SNR over the
sample rate. Complex baseband, so the spectrum has a single line at +0.10 (no
mirror); a red marker labels the peak.

Top-right — **PN** (maximum-length sequence, register length 7 → period 127)
at one chip per sample. Its periodic autocorrelation is the MLS "thumbtack":
1.0 at zero lag, a flat −1/127 floor everywhere else — the property that makes
an MLS a good spreading / ranging code.

Bottom-left — **QPSK** at 20 dB Es/No, after a boxcar matched filter over each
symbol. The four Gray-coded points sit at the corners; the noise cloud is the
symbol-energy SNR made visible.

Bottom-right — **BPSK** at 8 dB Es/No, matched-filtered. Two antipodal points on
the real axis with a wider cloud — lower SNR, fewer points.

Run:
    python examples/python/wfmgen_demo.py
"""

from __future__ import annotations

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from doppler.wfm import Synth

# ── tone: spectrum ──────────────────────────────────────────────────────────
N = 4096
tone = np.asarray(Synth(type="tone", fs=1.0, freq=0.10, snr=30.0).steps(N))
win = np.hanning(N)
spec = np.fft.fftshift(np.fft.fft(tone * win))
psd = 20.0 * np.log10(np.abs(spec) + 1e-12)
psd -= psd.max()
fax = np.fft.fftshift(np.fft.fftfreq(N))
peak = fax[np.argmax(psd)]

# ── PN: periodic autocorrelation of one MLS period (length 7 → 127) ─────────
period = 127
chips = np.asarray(
    Synth(type="pn", fs=1.0, freq=0.0, snr=100.0, sps=1, pn_length=7).steps(
        period
    )
).real
ac = np.array(
    [np.sum(chips * np.roll(chips, -k)) / period for k in range(period)]
)

# ── QPSK / BPSK: matched-filtered constellations ────────────────────────────
SPS = 8
NSYM = 600


def symbols(kind: str, snr: float) -> np.ndarray:
    x = np.asarray(
        Synth(
            type=kind, fs=1.0, freq=0.0, snr=snr, snr_mode="esno", sps=SPS
        ).steps(SPS * NSYM)
    )
    # boxcar matched filter: average each symbol's sps samples. This is the
    # receiver view that realises the Es/No (integrating the symbol energy
    # buys 10·log10(sps) dB over a single mid-symbol sample).
    return x.reshape(NSYM, SPS).mean(axis=1)


qpsk = symbols("qpsk", 20.0)
bpsk = symbols("bpsk", 8.0)

# ── plot ────────────────────────────────────────────────────────────────────
fig, ax = plt.subplots(2, 2, figsize=(11, 8))
fig.suptitle(
    "wfmgen engine — one declarative Synth, every waveform type",
    fontsize=13,
)

a = ax[0, 0]
a.plot(fax, psd, lw=0.8)
a.axvline(peak, color="r", ls="--", lw=1)
a.set_title(f"tone — spectrum (peak fn = {peak:+.3f})")
a.set_xlabel("normalised frequency (cycles/sample)")
a.set_ylabel("magnitude (dB)")
a.set_ylim(-80, 5)
a.grid(alpha=0.3)

a = ax[0, 1]
a.plot(np.arange(period), ac, lw=0.8)
a.axhline(-1.0 / period, color="r", ls="--", lw=1, label=f"-1/{period}")
a.set_title("PN (MLS, len 7) — periodic autocorrelation")
a.set_xlabel("lag (chips)")
a.set_ylabel("normalised correlation")
a.legend(loc="upper right")
a.grid(alpha=0.3)

for a, data, title in (
    (ax[1, 0], qpsk, "QPSK — constellation (20 dB Es/No)"),
    (ax[1, 1], bpsk, "BPSK — constellation (8 dB Es/No)"),
):
    a.scatter(data.real, data.imag, s=6, alpha=0.4)
    a.axhline(0, color="k", lw=0.5)
    a.axvline(0, color="k", lw=0.5)
    a.set_title(title)
    a.set_xlabel("I")
    a.set_ylabel("Q")
    a.set_xlim(-1.6, 1.6)
    a.set_ylim(-1.6, 1.6)
    a.set_aspect("equal")
    a.grid(alpha=0.3)

fig.tight_layout(rect=(0, 0, 1, 0.96))
fig.savefig("wfmgen_demo.png", dpi=110)
print("wrote wfmgen_demo.png")
