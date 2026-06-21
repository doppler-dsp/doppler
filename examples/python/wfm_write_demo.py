"""wfm_write_demo.py — generate a waveform and write it to a file.

Composes a short burst (tone → BPSK → silence) with the doppler wfm
Composer, writes it to a BLUE type-1000 container via ``Writer``, reads
it back with ``Reader``, and plots the time-domain magnitude of both.
The traces coincide — the codec is lossless for cf32.

Run::

    python examples/python/wfm_write_demo.py           # → burst.blue
    python examples/python/wfm_write_demo.py out.blue  # explicit path
"""

from __future__ import annotations

import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from doppler.wfm import Composer, Reader, Segment, Writer

FS = 1e6  # sample rate (Hz)
FC = 915e6  # centre frequency stored in the header

segments = [
    # 256-sample tone preamble at 100 kHz offset
    Segment(type="tone", freq=100e3, sps=1, fs=FS, snr=30.0, num_samples=256),
    # BPSK payload at 8 samples/symbol, 10 dB SNR, 512 symbols + silence
    Segment(
        type="bpsk",
        sps=8,
        fs=FS,
        snr=10.0,
        num_samples=512 * 8,
        off_samples=256,
    ),
]

burst = Composer(segments).compose()
path = sys.argv[1] if len(sys.argv) > 1 else "burst.blue"

with Writer(path, file_type="blue", fs=FS, fc=FC) as w:
    w.write(burst)

with Reader(path) as r:
    readback = r.read(len(burst))

t_ms = np.arange(len(burst)) / FS * 1e3

fig, ax = plt.subplots(figsize=(10, 4))
ax.plot(t_ms, np.abs(burst), lw=0.8, color="#1f77b4", label="writer in")
ax.plot(
    t_ms,
    np.abs(readback),
    lw=0.8,
    color="#ff7f0e",
    ls="--",
    label="reader out",
)
ax.set_xlabel("time (ms)")
ax.set_ylabel("|x|")
ax.set_title(
    f"wfm_write_demo — tone preamble | BPSK payload | silence  ({path})"
)
ax.legend(fontsize=9)
ax.grid(alpha=0.3)
fig.tight_layout()
fig.savefig("wfm_write_demo.png", dpi=110)

print(
    f"{len(burst)} samples, {len(burst) / FS * 1e3:.2f} ms"
    f"  →  {path}  →  wfm_write_demo.png"
)
