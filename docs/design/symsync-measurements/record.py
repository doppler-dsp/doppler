"""Record the timing loops' loop-filter output against time, and plot it.

``docs/design/symsync.md`` §11 shows one run of the harness
``native/validation/timing_loop_noise.c`` in its ``--trace`` mode: every
closed-loop scenario of the harness, on both timing loops (``RateSync`` and
``SymbolSync``), from the first strobe, started at the eye center (the loop
holding lock) and a quarter and a half symbol off it (the loop acquiring).
The run's data is kept beside this file:

- ``traces/<scenario>.csv``: for each scenario a figure draws
  (``FIGURES``), the harness's per-symbol trace reduced to ``ROWS``
  blocks. A held scenario keeps its whole length; an acquiring
  one keeps its first ``ACQ_TC / bn`` symbols, where the acquisition is.
  Each row is one block: its middle symbol, the mean, minimum and maximum
  of the loop filter's output, the mean of its integrator (all in symbols
  a symbol) and the mean timing error in symbols;
- ``scenarios.json``: every scenario's settings and its statistics,
  drawn or not, taken from the per-symbol trace before the reduction;
- ``meta.json``: the run's stamp (``_record_blocks.run_stamp``): when,
  where, from which commit and with which toolchain;
- ``hold_*.png``, ``acq_*.png``: the exemplary figures, drawn from
  ``traces/``.

The table of §11 is this file's output, held between
``<!-- symsync:<block>:start -->`` and ``:end`` markers, so a cell cannot
be edited by hand, or left behind by a re-run, without ``--check``
failing. Data from a dirty tree is refused.

Usage::

    python record.py measure     # the harness's --trace -> traces/, *.json
    python record.py figures     # traces/ -> *.png
    python record.py             # print the blocks
    python record.py --write     # rewrite them in the page
    python record.py --check     # fail when they differ (the gate)

``make symsync-record`` builds, measures, draws and writes; ``--check`` is
a line of the Makefile's ``DOCS_CHECK_PRE_CMDS``.
"""

from __future__ import annotations

import csv
import json
import math
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import _record_blocks as blocks
from _record_blocks import table

DATA = Path(__file__).resolve().parent
ROOT = DATA.parents[2]
PAGE = DATA.parent / "symsync.md"
SCRIPT = "docs/design/symsync-measurements/record.py"
SOURCE = ROOT / "native/validation/timing_loop_noise.c"
VALIDATOR = ROOT / "build/native/validation/validate_timing_loop_noise"
TRACES = DATA / "traces"
# Rows a kept trace holds.
ROWS = 300
# An acquiring scenario keeps its first ACQ_TC / bn symbols: four times the
# loop filter's own 5 / bn settling rule, so a slow acquisition still shows.
ACQ_TC = 20.0
# "Acquired" in the table: the timing error is inside this many symbols of
# the eye center it ends at, and stays there. A quarter symbol is the edge
# of the rectangle's S-curve, where the restoring force peaks.
ACQ_BAND = 0.25
PPM = 1e6
BLUE, ORANGE = "#2a78d6", "#eb6834"

OBJECTS = {"ratesync": "RateSync", "symsync": "SymbolSync"}
PULSES = {"iandd": "rectangle", "rrc": "root-raised cosine"}
ESN0S = ["none", "4dB", "10dB"]
NAME = re.compile(
    r"(?P<obj>ratesync|symsync)_(?P<pulse>iandd|rrc)_(?P<esn0>none|\d+dB)"
    r"_bn(?P<bn>[\d.]+)_a(?P<amp>[\d.]+)_eps(?P<eps>0|[pm]\de-\d+)"
    r"_off(?P<off>[\d.]+)$"
)


def parse(stem: str) -> dict[str, Any]:
    """A trace's name as its scenario's settings."""
    m = NAME.match(stem)
    if m is None:
        raise SystemExit(f"{stem}: not a scenario the harness names")
    return {
        "name": stem,
        "obj": m["obj"],
        "pulse": m["pulse"],
        "esn0": m["esn0"],
        "bn": float(m["bn"]),
        "amp": float(m["amp"]),
        "eps": float(m["eps"].replace("p", "+").replace("m", "-")),
        "off": float(m["off"]),
    }


def reduce(
    src: Path, s: dict[str, Any]
) -> tuple[list[list[float]], dict[str, Any]]:
    """One per-symbol trace as block rows, and its statistics.

    Over the second half: the output's mean and rms about it, the
    integrator's mean and the timing error's standard deviation. Over the
    whole trace: where the timing error starts and ends (the mean of the
    first 8 symbols and of the last quarter), and the first symbol from
    which it stays within ``ACQ_BAND`` of where it ends.
    """
    with src.open(encoding="utf-8", newline="") as f:
        raw = [[float(v) for v in r] for r in list(csv.reader(f))[1:]]
    keep = len(raw) if s["off"] == 0.0 else int(ACQ_TC / s["bn"])
    keep = min(keep, len(raw))
    blk_n = keep // ROWS
    rows = []
    for i in range(0, ROWS * blk_n, blk_n):
        blk = raw[i : i + blk_n]
        out = [r[1] for r in blk]
        rows.append(
            [
                blk[blk_n // 2][0],
                sum(out) / blk_n,
                min(out),
                max(out),
                sum(r[2] for r in blk) / blk_n,
                sum(r[3] for r in blk) / blk_n,
            ]
        )
    half = raw[len(raw) // 2 :]
    n = len(half)
    out_mean = sum(r[1] for r in half) / n
    tau_mean = sum(r[3] for r in half) / n
    tail = raw[-(len(raw) // 4) :]
    tau_end = sum(r[3] for r in tail) / len(tail)
    last_out = 0
    for i, r in enumerate(raw):
        if abs(r[3] - tau_end) >= ACQ_BAND:
            last_out = i + 1
    stats = {
        "symbols": len(raw),
        "block": blk_n,
        "out_mean": out_mean,
        "out_rms": math.sqrt(sum((r[1] - out_mean) ** 2 for r in half) / n),
        "integ_mean": sum(r[2] for r in half) / n,
        "sigma_tau": math.sqrt(sum((r[3] - tau_mean) ** 2 for r in half) / n),
        "tau_start": sum(r[3] for r in raw[:8]) / 8,
        "tau_end": tau_end,
        "acquired_at": last_out,
    }
    return rows, stats


def measure() -> int:
    """Run the built harness's --trace; keep the reduced traces, the
    scenarios' statistics and the stamp."""
    meta = blocks.run_stamp(ROOT, VALIDATOR, SOURCE)
    t0 = time.perf_counter()
    scenarios = []
    with tempfile.TemporaryDirectory() as tmp:
        run = subprocess.run(
            [str(VALIDATOR), "--trace", tmp], capture_output=True, text=True
        )
        if run.returncode != 0:
            sys.stderr.write(run.stdout + run.stderr)
            return run.returncode
        TRACES.mkdir(exist_ok=True)
        for old in TRACES.glob("*.csv"):
            old.unlink()
        for src in sorted(Path(tmp).glob("*.csv")):
            s = parse(src.stem)
            rows, stats = reduce(src, s)
            if s["name"] in DRAWN:
                with (TRACES / src.name).open(
                    "w", encoding="utf-8", newline=""
                ) as f:
                    w = csv.writer(f, lineterminator="\n")
                    w.writerow(
                        [
                            "symbol",
                            "out_mean",
                            "out_min",
                            "out_max",
                            "integ_mean",
                            "tau_mean",
                        ]
                    )
                    for r in rows:
                        w.writerow(
                            [f"{r[0]:.0f}"] + [f"{v:.5e}" for v in r[1:]]
                        )
            scenarios.append({**s, **stats})
    if not scenarios:
        raise SystemExit("the harness wrote no trace")
    meta["load_after"] = blocks.load_avg()
    meta["wall_s"] = round(time.perf_counter() - t0, 1)
    (DATA / "scenarios.json").write_text(
        json.dumps(scenarios, indent=1) + "\n", encoding="utf-8"
    )
    (DATA / "meta.json").write_text(
        json.dumps(meta, indent=1) + "\n", encoding="utf-8"
    )
    print(
        f"measure: {VALIDATOR.name} --trace, {len(scenarios)} scenarios "
        f"in {meta['wall_s']} s"
    )
    return 0


def load() -> tuple[list[dict[str, Any]], dict[str, Any]]:
    """The run and its stamp, held to a clean tree and a full toolchain.

    Each scenario gains ``center``: the eye center of its loop and pulse on
    the trace's timing axis, which is where that loop's held, noiseless
    scenario ends. A scenario's timing error is its trace less this.
    """
    meta = json.loads((DATA / "meta.json").read_text(encoding="utf-8"))
    scenarios = json.loads(
        (DATA / "scenarios.json").read_text(encoding="utf-8")
    )
    blocks.require_toolchain("meta.json", meta)
    if meta["dirty"]:
        raise SystemExit("record data from a dirty tree is refused")
    center = {
        (s["obj"], s["pulse"]): s["tau_end"]
        for s in scenarios
        if s["esn0"] == "none" and is_grid(s) and s["off"] == 0.0
    }
    names = {s["name"] for s in scenarios}
    for name in DRAWN:
        if name not in names:
            raise SystemExit(f"{name}: a figure draws a scenario not run")
        if not (TRACES / f"{name}.csv").exists():
            raise SystemExit(f"traces/{name}.csv is missing")
    for s in scenarios:
        s["center"] = center[(s["obj"], s["pulse"])]
    return scenarios, meta


def trace(name: str) -> list[list[float]]:
    """A kept scenario's block rows."""
    with (TRACES / f"{name}.csv").open(encoding="utf-8", newline="") as f:
        return [[float(v) for v in r] for r in list(csv.reader(f))[1:]]


def is_grid(s: dict[str, Any]) -> bool:
    """A scenario of the Es/N0 by bandwidth grid: unit level, no offset."""
    return s["amp"] == 1.0 and s["eps"] == 0.0


def key(
    obj: str,
    pulse: str,
    esn0: str,
    bn: float,
    off: float,
    amp: float = 1.0,
    eps: str = "0",
) -> str:
    """The name the harness gives a scenario."""
    return f"{obj}_{pulse}_{esn0}_bn{bn:.5f}_a{amp:.1f}_eps{eps}_off{off:.3f}"


def _figures() -> dict[str, dict[str, Any]]:
    """The exemplary figures: for each, its title, its y label, and its
    panels by row (one row a loop). A panel is the scenarios it draws:
    one for a loop-filter panel, the two starting offsets for a timing
    panel.

    The record keeps every scenario's statistics in the table; the figures
    show the case the DSSS receivers run (the rectangle, Es/N0 4 dB) and
    the two settings that move a loop's rate state (level and clock
    offset).
    """
    bns = [0.005, 0.002, 0.00068]
    figs: dict[str, dict[str, Any]] = {
        "hold_rectangle.png": {
            "title": "Holding lock on the rectangle: the loop filter's "
            "output, started at the eye center",
            "kind": "lf",
            "rows": [
                [
                    [key(o, "iandd", "none", 0.005, 0.0)],
                    [key(o, "iandd", "4dB", 0.005, 0.0)],
                    [key(o, "iandd", "4dB", 0.00068, 0.0)],
                ]
                for o in OBJECTS
            ],
        },
        "acq_timing_rectangle.png": {
            "title": "Acquiring on the rectangle at Es/N0 4 dB: the "
            "timing error",
            "kind": "tau",
            "rows": [
                [
                    [key(o, "iandd", "4dB", bn, off) for off in (0.25, 0.5)]
                    for bn in bns
                ]
                for o in OBJECTS
            ],
        },
        "acq_output_rectangle.png": {
            "title": "Acquiring on the rectangle at Es/N0 4 dB: the loop "
            "filter's output, started half a symbol off the eye center",
            "kind": "lf",
            "rows": [
                [[key(o, "iandd", "4dB", bn, 0.5)] for bn in bns]
                for o in OBJECTS
            ],
        },
        "hold_level_and_offset.png": {
            "title": "Level and clock offset on the root-raised cosine: "
            "the loop filter's output, started at the eye center",
            "kind": "lf",
            "rows": [
                [
                    [key(o, "rrc", "10dB", 0.002, 0.0, amp=0.5)],
                    [key(o, "rrc", "10dB", 0.002, 0.0, amp=2.0)],
                    [key(o, "rrc", "none", 0.005, 0.0, eps="p1e-03")],
                    [key(o, "rrc", "none", 0.005, 0.0, eps="m1e-03")],
                ]
                for o in OBJECTS
            ],
        },
    }
    return figs


FIGURES = _figures()
# Each drawn scenario's figure. The harness traces every scenario; only
# these are kept as traces.
DRAWN = {
    name: fig
    for fig, spec in FIGURES.items()
    for row in spec["rows"]
    for cell in row
    for name in cell
}


def esn0_text(s: dict[str, Any]) -> str:
    return "no noise" if s["esn0"] == "none" else f"Es/N0 {s['esn0'][:-2]} dB"


def settings(s: dict[str, Any]) -> str:
    """A scenario's settings as a panel's first title line."""
    text = f"{esn0_text(s)}, bn {s['bn']:g}"
    if not is_grid(s):
        text += f", A {s['amp']:g}, clock {s['eps'] * PPM:+.0f} ppm"
    return text


def lf_panel(ax: Any, s: dict[str, Any], prefix: str = "") -> None:
    """One scenario: the loop filter's output averaged over a block, and
    its integrator, in parts per million of the symbol rate."""
    rows = trace(s["name"])
    x = [r[0] / 1000.0 for r in rows]
    ax.plot(x, [r[1] * PPM for r in rows], color=BLUE, lw=1.0, label="output")
    ax.plot(
        x,
        [r[4] * PPM for r in rows],
        color=ORANGE,
        lw=2.0,
        label="integrator",
    )
    ax.axhline(0.0, color="#888888", lw=0.6)
    # The statistics go above the panel, one a line: inside it they sit
    # on the trace, and on one line they run into the next panel's.
    ax.set_title(
        f"{prefix}{settings(s)}\n"
        f"output is the mean of {s['block']} symbols; "
        f"{s['out_rms'] * PPM:.0f} ppm rms a symbol\n"
        f"integrator mean {s['integ_mean'] * PPM:+.1f} ppm, "
        f"timing error {s['sigma_tau']:.4f} symbols rms",
        fontsize=7.5,
    )
    ax.grid(True, color="#dddddd", lw=0.5)
    ax.tick_params(labelsize=7)


def tau_panel(ax: Any, pair: list[dict[str, Any]], prefix: str = "") -> None:
    """One scenario's acquisitions: the timing error against time from each
    starting offset, in symbols from the eye center."""
    for s, color in zip(pair, [BLUE, ORANGE]):
        rows = trace(s["name"])
        ax.plot(
            [r[0] / 1000.0 for r in rows],
            [r[5] - s["center"] for r in rows],
            color=color,
            lw=1.5,
            label=f"started {s['off']:g} symbol off",
        )
    for y in (0.0, -1.0):
        ax.axhline(y, color="#888888", lw=0.6)
    ax.set_title(
        f"{prefix}{settings(pair[0])}\n"
        + f"inside {ACQ_BAND:g} symbol for good after "
        + " and ".join(f"{s['acquired_at']} (from {s['off']:g})" for s in pair)
        + " symbols",
        fontsize=7.5,
    )
    ax.grid(True, color="#dddddd", lw=0.5)
    ax.tick_params(labelsize=7)


def figures() -> int:
    """Draw every figure of FIGURES from the kept traces."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    scenarios, meta = load()
    by_name = {s["name"]: s for s in scenarios}
    for old in DATA.glob("*.png"):
        old.unlink()
    for name, spec in FIGURES.items():
        rows = spec["rows"]
        ncol = len(rows[0])
        fig, axes = plt.subplots(
            len(rows), ncol, figsize=(4.4 * ncol, 7.4), squeeze=False
        )
        for r, (obj_name, row) in enumerate(zip(OBJECTS.values(), rows)):
            for c, cell in enumerate(row):
                drawn = [by_name[n] for n in cell]
                if spec["kind"] == "tau":
                    tau_panel(axes[r][c], drawn, f"{obj_name}: ")
                else:
                    lf_panel(axes[r][c], drawn[0], f"{obj_name}: ")
            axes[r][0].set_ylabel(
                "symbols from the eye center"
                if spec["kind"] == "tau"
                else "ppm of the symbol rate",
                fontsize=8,
            )
        for ax in axes[-1]:
            ax.set_xlabel("time, thousands of symbols", fontsize=8)
        axes[0][0].legend(fontsize=7, loc="upper right")
        fig.suptitle(f"{spec['title']} (commit {meta['commit']})", fontsize=10)
        fig.tight_layout()
        fig.savefig(DATA / name, dpi=100)
        plt.close(fig)
    print(f"figures: {', '.join(FIGURES)}")
    return 0


def scenarios_block(
    scenarios: list[dict[str, Any]], meta: dict[str, Any]
) -> str:
    """Every scenario: its settings and its statistics."""
    rows = []
    for s in scenarios:
        figs = (
            f"[plot](symsync-measurements/{DRAWN[s['name']]})"
            if s["name"] in DRAWN
            else "—"
        )
        rows.append(
            [
                OBJECTS[s["obj"]],
                PULSES[s["pulse"]],
                "none" if s["esn0"] == "none" else f"{s['esn0'][:-2]} dB",
                f"{s['bn']:g}",
                f"{s['amp']:g}",
                f"{s['eps'] * PPM:+.0f}" if s["eps"] else "0",
                f"{s['off']:g}",
                f"{s['tau_end'] - s['center']:+.2f}",
                "—" if s["off"] == 0.0 else f"{s['acquired_at']}",
                f"{s['integ_mean'] * PPM:+.1f}",
                f"{s['out_rms'] * PPM:.0f}",
                f"{s['sigma_tau']:.4f}",
                figs,
            ]
        )
    return table(
        [
            "loop",
            "pulse",
            "Es/N0",
            "bn",
            "level A",
            "clock, ppm",
            "started off center, symbols",
            "ends at, symbols from center",
            f"inside {ACQ_BAND:g} symbol for good after, symbols",
            "integrator mean, ppm",
            "output rms, ppm",
            "timing error rms, symbols",
            "figure",
        ],
        rows,
    )


def stamp(scenarios: list[dict[str, Any]], meta: dict[str, Any]) -> str:
    """The run: when, where, at which commit and toolchain."""
    lengths = sorted({s["symbols"] // 1000 for s in scenarios})
    return table(
        ["run", "when", "machine", "commit", "toolchain", "command"],
        [
            [
                "`validate_timing_loop_noise --trace`, "
                f"{len(scenarios)} scenarios of "
                f"{' or '.join(f'{n},000' for n in lengths)} symbols, "
                "one seed",
                f"{meta['date_utc']} UTC, {meta['wall_s']:.0f} s",
                f"`{meta['machine']}`: {meta['cpu_model']} "
                f"({meta['cpu_threads']} threads)",
                f"`{meta['commit']}`, clean",
                blocks.tc_cell(meta),
                "`make symsync-record`",
            ]
        ],
    )


BLOCKS = {"scenarios": scenarios_block, "stamp": stamp}


def render() -> dict[str, str]:
    data = load()
    return {name: f(*data) for name, f in BLOCKS.items()}


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "measure":
        sys.exit(measure())
    if len(sys.argv) > 1 and sys.argv[1] == "figures":
        sys.exit(figures())
    sys.exit(
        blocks.main(
            PAGE,
            "symsync",
            "symsync loop-filter traces",
            SCRIPT,
            render,
            __doc__ or "",
        )
    )
