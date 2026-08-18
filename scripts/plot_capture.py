#!/usr/bin/env python3
"""Plot any filed telemetry capture. One utility, every subject.

A capture is a self-describing artifact — a 16-byte record stream plus a JSON
sidecar naming every probe (`dp_tlm_capture_core.h`) — which is exactly the
property that makes one plotter enough. Before this there was one script per
subject, and `plot_rx_dynamics.py` was the only one that existed, wired to a
single validator and a single output path.

**This computes nothing.** Every value drawn is a record the object itself
emitted, on the sample index it stamped. That is the rule the prototype states
and the one worth keeping: a plotting script that re-derived the dynamics
would be a second implementation of the thing under test, and the two would
drift. This one cannot disagree with the evidence, because the evidence is its
only input.

It reads FILED captures only — the ones committed under an object's
`data/`, written by `Report.capture` (doppler#846). It never runs a
validator, so the picture and the report cannot come from different runs.

Usage
-----
    python scripts/plot_capture.py <capture.tlm> [-o out.png] [probe ...]
    python scripts/plot_capture.py <capture.tlm> --list

With no probe names every probe is drawn, one panel per probe, sharing the
sample-index axis. Names may be given with or without the attach prefix, so
`car.freq` and `rx.car.freq` both select the same series.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "src"))

from doppler.tests._validation_common import read_capture  # noqa: E402


def _match(available, wanted):
    """Resolve requested names against the capture's own probe table.

    Suffix matching is deliberate: a capture records probes under the name the
    object was attached with (`rx.car.freq`), while a reader thinks in the
    object's own terms (`car.freq`). Requiring the prefix would make every
    command line depend on a string chosen by whoever wrote the validator.
    """
    if not wanted:
        return list(available)
    out = []
    for w in wanted:
        hits = [n for n in available if n == w or n.endswith("." + w)]
        if not hits:
            raise SystemExit(
                f"no probe matching {w!r}. This capture carries:\n  "
                + "\n  ".join(sorted(available))
            )
        out.extend(hits)
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("capture", type=Path, help="a filed <name>.tlm")
    ap.add_argument("probes", nargs="*", help="probe names; default all")
    ap.add_argument("-o", "--out", type=Path, help="output PNG")
    ap.add_argument(
        "--list",
        action="store_true",
        help="print the probe table and exit — the capture describes itself",
    )
    args = ap.parse_args(argv)

    if not args.capture.exists():
        raise SystemExit(f"no such capture: {args.capture}")
    series = read_capture(args.capture)

    if args.list:
        for name in sorted(series):
            print(f"{name:<24} {len(series[name]):>8} records")
        return 0

    names = _match(series, args.probes)
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(
        len(names),
        1,
        figsize=(10, 1.9 * len(names)),
        sharex=True,
        squeeze=False,
    )
    for ax, name in zip(axes[:, 0], names):
        # A probe carrying recovered SYMBOLS is a set of discrete decisions,
        # one per symbol period, with nothing in between to interpolate
        # through. A line draws a path the object never took -- on a BPSK
        # stream toggling +-1 it fills the panel solid and shows nothing,
        # where dots show the rails, the acquisition transient and the noise
        # cloud collapsing as the carrier locks. Loop state is the opposite: a
        # continuous quantity sampled per symbol, so it stays a line.
        if ".sym." in name or name.endswith((".i", ".q")):
            ax.plot(series[name], ".", ms=1.5)
        else:
            ax.plot(series[name], lw=0.9)
        ax.set_ylabel(name.split(".", 1)[-1], fontsize=8)
        ax.grid(alpha=0.3)
    axes[-1, 0].set_xlabel("record index (per probe)")
    axes[0, 0].set_title(f"{args.capture.name} — {len(names)} probe(s)")
    fig.tight_layout()

    out = args.out or args.capture.with_suffix(".png")
    fig.savefig(out, dpi=110)
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
