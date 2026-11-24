"""Plot what ``ccsds415`` captured: one figure per carrier.

Usage::

    python3 plot.py OUT_DIR [--save]

Reads every ``OUT_DIR/*.tlm`` with its ``*.tlm-meta`` sidecar and plots, per
carrier:

- the lifecycle and the lock flags (``rx.state``, ``rx.car.locked``,
  ``rx.code.locked``, ``rx.sym.locked``);
- the Doppler the tracking chain was built from (``rx.handover_hz``) and the
  carrier loop's frequency memory (``rx.car.freq``, as the receiver reports
  it);
- the loop discriminators (``rx.car.e``, ``rx.code.e``, ``rx.demod.car.e``,
  ``rx.demod.sync.e``);
- the recovered symbols, I against Q (``rx.demod.sym.i``, ``.q``);
- each chunk's processing time per sample (``app.proc_ns``) against the
  sample period.

It only plots. It needs numpy and matplotlib, not the doppler wheel, which
an RPM consumer does not have. The format is read from the sidecar, never
restated here: the record layout is the sidecar's ``dtype`` (the 16-byte
``dp_tlm_rec_t`` of ``doppler/dp_tlm/dp_tlm_core.h``), the probe names are
its ``probes`` table, and a record's time is ``n / fs`` with ``fs`` the
sidecar's sample clock.

With ``--save`` it writes ``<capture>.png`` beside each capture and shows
nothing, so it runs headless.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import TYPE_CHECKING

import numpy as np

if TYPE_CHECKING:
    from matplotlib.figure import Figure


def load(path: Path) -> tuple[dict[str, np.ndarray], dict]:
    """A capture's records, split by probe name: ``{name: records}``."""
    meta = json.loads(Path(f"{path}-meta").read_text(encoding="utf-8"))
    dtype = np.dtype([(name, fmt) for name, fmt in meta["dtype"]])
    recs = np.fromfile(path, dtype=dtype)
    by_name = {
        name: recs[recs["probe"] == pid]
        for name, pid in meta["probes"].items()
    }
    return by_name, meta


def figure(path: Path) -> Figure:
    """One carrier's figure."""
    import matplotlib.pyplot as plt

    p, meta = load(path)
    fs = float(meta["fs"])

    def t(name: str) -> np.ndarray:
        return p[name]["n"] / fs

    def v(name: str) -> np.ndarray:
        return p[name]["value"]

    fig, ax = plt.subplots(3, 2, figsize=(12, 10))
    fig.suptitle(path.stem)

    a = ax[0, 0]
    a.step(t("rx.state"), v("rx.state"), where="post", label="state")
    for k, name in enumerate(
        ("rx.car.locked", "rx.code.locked", "rx.sym.locked")
    ):
        a.step(
            t(name),
            v(name) + 1.1 * (k + 1),
            where="post",
            label=name.removeprefix("rx."),
        )
    a.set_title("lifecycle and lock flags (offset for clarity)")
    a.set_xlabel("time (s)")
    a.legend(loc="lower right", fontsize="small")

    a = ax[0, 1]
    a.plot(t("rx.handover_hz"), v("rx.handover_hz"), ".", label="hand-over")
    a.set_title("Doppler the chain was built from (Hz)")
    a.set_xlabel("time (s)")
    b = a.twinx()
    b.plot(t("rx.car.freq"), v("rx.car.freq"), "C1", lw=0.8)
    b.set_ylabel("rx.car.freq (as reported)", color="C1")

    a = ax[1, 0]
    for name in ("rx.car.e", "rx.code.e"):
        a.plot(t(name), v(name), lw=0.6, label=name)
    a.set_title("chip-domain loop discriminators")
    a.set_xlabel("time (s)")
    a.legend(fontsize="small")

    a = ax[1, 1]
    for name in ("rx.demod.car.e", "rx.demod.sync.e"):
        a.plot(t(name), v(name), lw=0.6, label=name)
    a.set_title("symbol-domain loop discriminators")
    a.set_xlabel("time (s)")
    a.legend(fontsize="small")

    a = ax[2, 0]
    a.plot(v("rx.demod.sym.i"), v("rx.demod.sym.q"), ".", ms=1, alpha=0.3)
    a.set_title("recovered symbols")
    a.set_xlabel("I")
    a.set_ylabel("Q")
    a.set_aspect("equal", adjustable="datalim")

    a = ax[2, 1]
    a.plot(
        t("app.proc_ns"),
        v("app.proc_ns"),
        lw=0.6,
        label="processing time per sample",
    )
    a.axhline(1e9 / fs, color="C3", ls="--", label="sample period")
    a.set_yscale("log")
    a.set_title("cost per chunk (ns)")
    a.set_xlabel("time (s)")
    a.legend(fontsize="small")

    fig.tight_layout()
    return fig


def main(argv: list[str]) -> int:
    args = [a for a in argv if not a.startswith("--")]
    save = "--save" in argv
    if len(args) != 1:
        print(__doc__)
        return 2
    import matplotlib

    if save:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    captures = sorted(Path(args[0]).glob("*.tlm"))
    if not captures:
        print(f"no captures in {args[0]}: run `make run` first")
        return 1
    for path in captures:
        fig = figure(path)
        if save:
            fig.savefig(path.with_suffix(".png"), dpi=100)
            print(f"wrote {path.with_suffix('.png')}")
    if not save:
        plt.show()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
