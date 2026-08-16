#!/usr/bin/env python3
"""Plot the receiver dynamics captured by ``validate_rx_dynamics``.

The measurement is C and stays C: this script runs the built validator with
``--out``, reads the ``dp_tlm_capture`` files it wrote, and renders them. It
computes nothing about the receiver — every value plotted is a telemetry
record the receiver itself emitted, on the sample index it stamped.

That split is the point. A plotting script that re-derived the dynamics in
Python would be a second implementation of the thing under test, and the two
would drift; this one cannot disagree with the gate, because it reads the
gate's own output.

The 16-byte record layout IS the file (``n:u8, value:f4, probe:u2,
flags:u2``) plus a JSON sidecar naming the probes, so the read-back needs
nothing doppler-specific — see ``dp_tlm_capture_core.h``.

Writes ``docs/assets/rx-dynamics.png``.

Run:  make plot-rx-dynamics
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
VALIDATOR = ROOT / "build" / "native" / "validation" / "validate_rx_dynamics"
OUT = ROOT / "docs" / "assets" / "rx-dynamics.png"

# The record layout, straight from the header's own doctest.
DT = np.dtype(
    {
        "names": ["n", "value", "probe", "flags"],
        "formats": ["<u8", "<f4", "<u2", "<u2"],
        "offsets": [0, 8, 12, 14],
        "itemsize": 16,
    }
)

TAPS = ("strobe", "mf_out", "mf_in")
COLORS = {"strobe": "#1565c0", "mf_out": "#ef6c00", "mf_in": "#6a1b9a"}

# Kept in step with rx_dynamics.c. Only used for the time axis and the onset
# marker — nothing measured is recomputed here.
FS = 8000.0
ONSET_SYM = 12000
SPS = 8
RAMP_HZ_S = 2.0e-4 * 1e-6 * 2.4e9  # rx_dynamics.c's RATE_PPM_S x CARRIER_HZ
SMOOTH = 251  # symbols; see the comment where it is used


def smooth(n: np.ndarray, v: np.ndarray, w: int) -> tuple:
    """Centred rolling mean, for legibility only — never for a number."""
    if v.size < w:
        return n, v
    k = np.ones(w) / w
    return n[w // 2 : -(w // 2)], np.convolve(v, k, mode="valid")[
        : n.size - w + 1
    ]


def load(d: Path, tap: str) -> tuple[dict[str, tuple], dict]:
    """Read one capture into ``{probe_name: (sample_index, value)}``."""
    meta = json.loads((d / f"rx-dyn-{tap}.tlm-meta").read_text())
    rec = np.fromfile(d / f"rx-dyn-{tap}.tlm", dtype=DT)
    if meta["dropped"]:
        raise SystemExit(f"{tap}: capture dropped {meta['dropped']} records")
    out = {}
    for name, pid in meta["probes"].items():
        m = rec["probe"] == pid
        out[name] = (
            rec["n"][m].astype(float) / FS,
            rec["value"][m].astype(float),
        )
    return out, meta


def main() -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    if not VALIDATOR.exists():
        raise SystemExit(f"{VALIDATOR} not built — run `make build` first")

    with tempfile.TemporaryDirectory() as td:
        d = Path(td)
        subprocess.run([str(VALIDATOR), "--out", str(d)], check=True)
        series = {tap: load(d, tap)[0] for tap in TAPS}

    onset_s = ONSET_SYM * SPS / FS
    rows = [
        ("rx.lock", "carrier lock\n(I²−Q²)/(I²+Q²), EMA", None),
        ("rx.car.nco", "carrier command\ncycles/sample", None),
        ("rx.sync.rate", "tracked rate\nsamples/symbol", (7.996, 8.004)),
    ]
    fig, axes = plt.subplots(len(rows), 1, figsize=(11.0, 7.5), sharex=True)

    for ax, (probe, label, ylim) in zip(axes, rows):
        for tap in TAPS:
            n, v = series[tap][probe]
            # A rolling mean over ~1/4 of the loop's own settling time. At
            # decim=1 the raw trace is 100k+ points of per-symbol variance,
            # which renders as a solid band and hides the very transient the
            # figure exists to show. The window is far shorter than the
            # dynamics (5/bn = 1000 symbols), so nothing measured is smoothed
            # away -- only the per-sample noise the EMA has not yet averaged.
            n_s, v_s = smooth(n, v, SMOOTH)
            ax.plot(n, v, lw=0.4, color=COLORS[tap], alpha=0.12)
            ax.plot(n_s, v_s, lw=1.4, color=COLORS[tap], label=tap)
        ax.axvline(onset_s, color="#b71c1c", lw=1.2, ls="--", zorder=5)
        ax.set_ylabel(label, fontsize=8)
        ax.grid(alpha=0.25)
        if ylim:
            ax.set_ylim(*ylim)
        ax.margins(x=0)

    # The carrier panel gets the EXTERNAL TRUTH drawn on it: the ramp the
    # channel was told to apply. The gate scores against this same line, so
    # the figure shows what is gated rather than an unanchored curve.
    tt = np.linspace(0.0, ONSET_SYM * 2 * SPS / FS, 200)
    axes[1].plot(
        tt,
        RAMP_HZ_S * tt / FS,
        color="#455a64",
        lw=1.1,
        ls=":",
        label="ramp applied (truth)",
    )
    axes[1].legend(loc="upper left", fontsize=8)

    axes[0].legend(loc="lower right", fontsize=8, ncol=3)
    axes[0].annotate(
        "data onset — transitions begin,\ntiming can close for the first time",
        xy=(onset_s, 0.55),
        xytext=(onset_s - 9.0, 0.30),
        fontsize=8,
        color="#b71c1c",
        arrowprops={"arrowstyle": "->", "color": "#b71c1c", "lw": 0.9},
    )
    axes[-1].set_xlabel(
        "time (s)  —  modulation OFF until the marker", fontsize=9
    )
    fig.suptitle(
        "MpskReceiver under a coupled Doppler ramp across a data onset\n"
        "NRZ BPSK, I&D, m_out=4, DTTL, Es/N0 12 dB, 0.48 Hz/s "
        "(carrier and every clock together)",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    OUT.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUT, dpi=110)
    print(f"wrote {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    sys.exit(main())
