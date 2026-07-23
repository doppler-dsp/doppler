"""wfm_io_demo.py — write a waveform once, read it back from any container.

The waveform I/O layer (``doppler.wfm.Writer`` / ``Reader``)
is the
same C codec behind the ``wfmgen`` CLI's ``--file-type``. This demo writes one
QPSK capture to all four containers and reads each back, showing the headline
distinction between them:

- **raw** — bare interleaved I/Q. Smallest, fastest, but carries
  **no metadata**:
  the reader must be *told* the sample type (and ``fs``/``fc`` are not stored).
- **csv** — human-readable ``I,Q`` text. Self-describing shape, no metadata,
  big.
- **BLUE (type-1000)** — a 512-byte header that records sample type,
  byte order,
  ``fs`` and ``fc``; the reader recovers them with no hints.
- **SigMF** — a ``.sigmf-data`` + ``.sigmf-meta`` JSON sidecar, likewise
  self-describing (``fs``/``fc`` recovered from the metadata).

Each panel overlays the round-tripped spectrum on the original (they coincide —
the codec is lossless for ``cf32``) and annotates the on-disk size and the
metadata the container recovered. ``Reader`` auto-detects the container; raw is
the one case that needs a ``sample_type`` hint.

Run:
    python examples/python/wfm_io_demo.py
"""

from __future__ import annotations

import os
import tempfile

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from doppler.spectral import PSD
from doppler.wfm import Composer, Reader, Segment, Writer

FS = 1e6
FC = 2.4e9
N = 1 << 14


def psd_db(x, nfft=1024):
    """Averaged PSD (dB, DC-centred), normalised to its own peak.

    Thin call to the C-backed ``doppler.spectral.PSD`` — Welch averaging with
    a Hann window, the same estimator the analyzer/measure suite is built on.
    """
    est = PSD(n=nfft, fs=FS, window="hann", mode="mean")
    est.accumulate(np.asarray(x, dtype=np.complex64))
    p = est.psd_db()
    return p - p.max()


# ── the capture: a QPSK signal at +150 kHz, written with fs/fc tagged ────────
# Keep it as an explicit Segment so SigMF can annotate it (Composer.to_sigmf
# annotates each resolved segment).
seg = Segment("qpsk", fs=FS, freq=1.5e5, snr=20.0, sps=8, num_samples=N)
x = Composer([seg]).compose()

work = tempfile.mkdtemp()
# (file_type, filename, how to open the Reader). Raw has no magic/metadata, so
# the reader is given the sample type; the rest auto-detect from the container.
formats = [
    ("raw", "cap.cf32", {"sample_type": "cf32"}),
    ("csv", "cap.csv", {}),
    ("blue", "cap.blue", {}),
    ("sigmf", "cap.sigmf-data", {}),
]

results = []
for file_type, name, read_kw in formats:
    path = os.path.join(work, name)
    with Writer(
        path, file_type=file_type, sample_type="cf32", fs=FS, fc=FC
    ) as w:
        w.write(x)
    # SigMF is a pair: the Writer lays down the .sigmf-data samples; the
    # .sigmf-meta JSON sidecar (sample rate, datatype, per-segment annotations)
    # is written alongside it so the Reader can auto-detect and recover fs/fc.
    if file_type == "sigmf":
        meta_path = path.replace(".sigmf-data", ".sigmf-meta")
        with open(meta_path, "w") as fh:
            fh.write(
                Composer([seg]).to_sigmf(sample_type="cf32", fs=FS, fc=FC)
            )
    with Reader(path, **read_kw) as r:
        y = r.read(N)  # read the whole capture (N < one read block)
        meta = {"file_type": r.file_type, "fs": r.fs, "fc": r.fc}
    size = os.path.getsize(path)
    err = float(np.max(np.abs(y - x))) if len(y) == len(x) else float("nan")
    results.append((file_type, y, size, meta, err))

# ── plot: one panel per container, round-trip overlaid + metadata recovered ──
f = np.linspace(-0.5, 0.5, 1024) * FS / 1e3  # kHz
orig_db = psd_db(x)
fig, axes = plt.subplots(2, 2, figsize=(12, 9))
fig.suptitle(
    "wfm I/O — one waveform, four containers, lossless round-trip",
    fontsize=14,
    fontweight="bold",
)
for ax, (file_type, y, size, meta, err) in zip(axes.ravel(), results):
    ax.plot(f, orig_db, lw=1.6, color="#bbbbbb", label="original")
    ax.plot(f, psd_db(y), lw=0.8, color="#1f77b4", label="read back")
    ax.set_title(f"{file_type}  ·  {size / 1024:.0f} KiB on disk")
    ax.set_xlabel("frequency (kHz)")
    ax.set_ylabel("dB (rel. peak)")
    ax.set_ylim(-80, 5)
    ax.grid(alpha=0.3)
    # fs/fc recovered from the container (raw/csv store none → 0 / not tagged)
    fs_txt = f"{meta['fs'] / 1e6:.1f} MS/s" if meta["fs"] else "— (hint)"
    fc_txt = f"{meta['fc'] / 1e9:.2f} GHz" if meta["fc"] else "— (not stored)"
    ax.text(
        0.03,
        0.05,
        f"detected: {meta['file_type']}\n"
        f"fs: {fs_txt}\nfc: {fc_txt}\n"
        f"round-trip |Δ|max: {err:.1e}",
        transform=ax.transAxes,
        fontsize=8,
        va="bottom",
        family="monospace",
        bbox={
            "boxstyle": "round",
            "fc": "white",
            "ec": "#cccccc",
            "alpha": 0.9,
        },
    )
    ax.legend(loc="upper right", fontsize=8)

fig.tight_layout(rect=[0, 0, 1, 0.97])
fig.savefig("wfm_io_demo.png", dpi=110)
print("Wrote the same QPSK capture to four containers, read each back:")
for file_type, _, size, meta, err in results:
    print(
        f"  {file_type:5s}  {size / 1024:6.0f} KiB"
        f"  detected={meta['file_type']:5s}"
        f"  fs={meta['fs'] / 1e6 or 0:.1f}M  fc={meta['fc'] / 1e9 or 0:.2f}G"
        f"  |Δ|max={err:.1e}"
    )
print("→ wfm_io_demo.png")

# ── validate ─────────────────────────────────────────────────────────────────
metas = {}
for file_type, y, _size, meta, err in results:
    # Every container returns the full capture, auto-detected (raw hinted).
    assert len(y) == N, f"{file_type}: read {len(y)} of {N} samples"
    assert meta["file_type"] == file_type, f"{file_type} detected as {meta}"
    if file_type == "csv":
        # Text container: samples round-trip through their decimal repr —
        # not bit-exact, but well inside a float32 ulp of full scale.
        assert err < 1e-6, f"csv round-trip error {err:.1e}"
    else:
        # Binary cf32 containers are bit-exact.
        assert err == 0.0, f"{file_type} round-trip error {err:.1e}"
    metas[file_type] = meta
# Self-describing containers recover the tagged sample rate with no hints;
# SigMF's JSON sidecar also carries the centre frequency. Raw stores no
# metadata at all — its fs must come back unset (the reader was told).
assert metas["blue"]["fs"] == FS and metas["sigmf"]["fs"] == FS
assert metas["sigmf"]["fc"] == FC
assert not metas["raw"]["fs"], "raw container should carry no fs"
print(
    "validated: 4 containers round-trip losslessly, "
    "blue/sigmf recover fs, sigmf recovers fc"
)
