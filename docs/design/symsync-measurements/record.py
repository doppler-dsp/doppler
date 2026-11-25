"""Record the timing loops' loop-filter output against time, and plot it.

``docs/design/symsync.md`` §11 shows one run of the harness
``native/validation/timing_loop_noise.c`` in its ``--trace`` mode: every
closed-loop scenario of the harness, on both timing loops (``RateSync`` and
``SymbolSync``), from the first strobe. The run's data is kept beside this
file:

- ``traces/<scenario>.csv``: the harness's per-symbol trace reduced to
  blocks of ``BLOCK`` symbols. Each row is one block: its middle symbol,
  the mean, minimum and maximum of the loop filter's output, the mean of
  its integrator (all in symbols a symbol) and the mean timing error in
  symbols;
- ``scenarios.json``: each scenario's settings and its statistics over the
  second half of the per-symbol trace, taken before the reduction;
- ``meta.json``: the run's stamp (``_record_blocks.run_stamp``): when,
  where, from which commit and with which toolchain;
- ``lf_*.png``: the figures, drawn from ``traces/``.

The table of §11 is this file's output, held between
``<!-- symsync:<block>:start -->`` and ``:end`` markers, so a cell cannot
be edited by hand, or left behind by a re-run, without ``--check``
failing. Data from a dirty tree is refused.

Usage::

    python record.py measure     # the harness's --trace -> traces/, *.json
    python record.py figures     # traces/ -> lf_*.png
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
# Symbols a kept row averages. The harness traces 60,000 symbols a
# scenario, so a scenario keeps 500 rows.
BLOCK = 120
PPM = 1e6

OBJECTS = {"ratesync": "RateSync", "symsync": "SymbolSync"}
PULSES = {"iandd": "rectangle", "rrc": "root-raised cosine"}
NAME = re.compile(
    r"(?P<obj>ratesync|symsync)_(?P<pulse>iandd|rrc)_(?P<esn0>none|\d+dB)"
    r"_bn(?P<bn>[\d.]+)_a(?P<amp>[\d.]+)_eps(?P<eps>0|[pm]\de-\d+)$"
)


def kept_name(stem: str) -> str:
    """The name a harness trace is kept under. The harness writes a clock
    offset's sign as ``+`` or ``-``; a tracked path holds neither a ``+``
    nor a doubled sign, so the kept name spells them ``p`` and ``m``."""
    return stem.replace("eps+", "epsp").replace("eps-", "epsm")


def parse(stem: str) -> dict[str, Any]:
    """A kept trace's name as its scenario's settings."""
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
    }


def reduce(src: Path) -> tuple[list[list[float]], dict[str, float]]:
    """One per-symbol trace as block rows, and its second half's
    statistics: the output's mean and rms about it, the integrator's mean
    and the timing error's standard deviation."""
    with src.open(encoding="utf-8", newline="") as f:
        raw = [[float(v) for v in r] for r in list(csv.reader(f))[1:]]
    rows = []
    for i in range(0, len(raw) - BLOCK + 1, BLOCK):
        blk = raw[i : i + BLOCK]
        out = [r[1] for r in blk]
        rows.append(
            [
                blk[BLOCK // 2][0],
                sum(out) / BLOCK,
                min(out),
                max(out),
                sum(r[2] for r in blk) / BLOCK,
                sum(r[3] for r in blk) / BLOCK,
            ]
        )
    half = raw[len(raw) // 2 :]
    n = len(half)
    out_mean = sum(r[1] for r in half) / n
    tau_mean = sum(r[3] for r in half) / n
    stats = {
        "symbols": len(raw),
        "out_mean": out_mean,
        "out_rms": math.sqrt(sum((r[1] - out_mean) ** 2 for r in half) / n),
        "integ_mean": sum(r[2] for r in half) / n,
        "sigma_tau": math.sqrt(sum((r[3] - tau_mean) ** 2 for r in half) / n),
        "tau_span": max(r[3] for r in raw) - min(r[3] for r in raw),
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
            rows, stats = reduce(src)
            name = kept_name(src.stem)
            with (TRACES / f"{name}.csv").open(
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
                    w.writerow([f"{r[0]:.0f}"] + [f"{v:.6e}" for v in r[1:]])
            scenarios.append({**parse(name), **stats})
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
    """The run and its stamp, held to a clean tree and a full toolchain."""
    meta = json.loads((DATA / "meta.json").read_text(encoding="utf-8"))
    scenarios = json.loads(
        (DATA / "scenarios.json").read_text(encoding="utf-8")
    )
    blocks.require_toolchain("meta.json", meta)
    if meta["dirty"]:
        raise SystemExit("record data from a dirty tree is refused")
    for s in scenarios:
        if not (TRACES / f"{s['name']}.csv").exists():
            raise SystemExit(f"traces/{s['name']}.csv is missing")
    return scenarios, meta


def trace(name: str) -> list[list[float]]:
    """A kept scenario's block rows."""
    with (TRACES / f"{name}.csv").open(encoding="utf-8", newline="") as f:
        return [[float(v) for v in r] for r in list(csv.reader(f))[1:]]


def is_grid(s: dict[str, Any]) -> bool:
    """A scenario of the Es/N0 by bandwidth grid: unit level, no offset."""
    return s["amp"] == 1.0 and s["eps"] == 0.0


def figure_of(s: dict[str, Any]) -> str:
    """The figure a scenario is drawn in."""
    if is_grid(s):
        return f"lf_{s['obj']}_{s['pulse']}.png"
    return "lf_level_and_offset.png"


def esn0_text(s: dict[str, Any]) -> str:
    return "no noise" if s["esn0"] == "none" else f"Es/N0 {s['esn0'][:-2]} dB"


def panel(ax: Any, s: dict[str, Any]) -> None:
    """One scenario: the loop filter's output averaged over BLOCK symbols,
    and its integrator, in parts per million of the symbol rate."""
    rows = trace(s["name"])
    x = [r[0] / 1000.0 for r in rows]
    ax.plot(
        x,
        [r[1] * PPM for r in rows],
        color="#2a78d6",
        lw=1.0,
        label=f"output, mean of {BLOCK} symbols",
    )
    ax.plot(
        x,
        [r[4] * PPM for r in rows],
        color="#eb6834",
        lw=2.0,
        label="integrator",
    )
    ax.axhline(0.0, color="#888888", lw=0.6)
    title = f"{esn0_text(s)}, bn {s['bn']:g}"
    if not is_grid(s):
        title += f", A {s['amp']:g}, clock {s['eps'] * PPM:+.0f} ppm"
    # The statistics go above the panel, one a line: inside it they sit
    # on the trace, and on one line they run into the next panel's.
    ax.set_title(
        f"{title}\n"
        f"output {s['out_rms'] * PPM:.0f} ppm rms a symbol\n"
        f"integrator mean {s['integ_mean'] * PPM:+.1f} ppm, "
        f"timing error {s['sigma_tau']:.4f} symbols rms",
        fontsize=7.5,
    )
    ax.grid(True, color="#dddddd", lw=0.5)
    ax.tick_params(labelsize=7)


def figures() -> int:
    """Draw every figure from the kept traces."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    scenarios, meta = load()
    by_name = {s["name"]: s for s in scenarios}
    bns = sorted({s["bn"] for s in scenarios if is_grid(s)}, reverse=True)
    made = []
    for obj, obj_name in OBJECTS.items():
        for pulse, pulse_name in PULSES.items():
            fig, axes = plt.subplots(
                3, len(bns), figsize=(13, 9.6), sharex=True, squeeze=False
            )
            for r, esn0 in enumerate(["none", "4dB", "10dB"]):
                for c, bn in enumerate(bns):
                    key = f"{obj}_{pulse}_{esn0}_bn{bn:.5f}_a1.0_eps0"
                    if key not in by_name:
                        axes[r][c].set_visible(False)
                        continue
                    panel(axes[r][c], by_name[key])
            for ax in axes[-1]:
                ax.set_xlabel("time, thousands of symbols", fontsize=8)
            for row in axes:
                row[0].set_ylabel("ppm of the symbol rate", fontsize=8)
            axes[0][0].legend(fontsize=7, loc="upper right")
            fig.suptitle(
                f"{obj_name}, {pulse_name}, Gardner: the loop filter's "
                f"output from the first strobe (commit {meta['commit']})",
                fontsize=10,
            )
            fig.tight_layout()
            out = DATA / f"lf_{obj}_{pulse}.png"
            fig.savefig(out, dpi=110)
            plt.close(fig)
            made.append(out.name)
    extra = [s for s in scenarios if not is_grid(s)]
    per_obj = [[s for s in extra if s["obj"] == o] for o in OBJECTS]
    width = max(len(r) for r in per_obj)
    fig, axes = plt.subplots(
        len(OBJECTS), width, figsize=(15, 7.4), sharex=True, squeeze=False
    )
    for r, (obj_name, row) in enumerate(zip(OBJECTS.values(), per_obj)):
        for c in range(width):
            if c >= len(row):
                axes[r][c].set_visible(False)
                continue
            panel(axes[r][c], row[c])
            axes[r][c].set_title(
                f"{obj_name}: " + axes[r][c].get_title(), fontsize=7.5
            )
        axes[r][0].set_ylabel("ppm of the symbol rate", fontsize=8)
    for ax in axes[-1]:
        ax.set_xlabel("time, thousands of symbols", fontsize=8)
    axes[0][0].legend(fontsize=7, loc="upper right")
    fig.suptitle(
        "Root-raised cosine, Gardner: level and clock offset "
        f"(commit {meta['commit']})",
        fontsize=10,
    )
    fig.tight_layout()
    fig.savefig(DATA / "lf_level_and_offset.png", dpi=110)
    plt.close(fig)
    made.append("lf_level_and_offset.png")
    print(f"figures: {', '.join(made)}")
    return 0


def scenarios_block(
    scenarios: list[dict[str, Any]], meta: dict[str, Any]
) -> str:
    """Every scenario: its settings and its second half's statistics."""
    rows = []
    for s in scenarios:
        rows.append(
            [
                OBJECTS[s["obj"]],
                PULSES[s["pulse"]],
                "none" if s["esn0"] == "none" else f"{s['esn0'][:-2]} dB",
                f"{s['bn']:g}",
                f"{s['amp']:g}",
                f"{s['eps'] * PPM:+.0f}" if s["eps"] else "0",
                f"{s['out_mean'] * PPM:+.1f}",
                f"{s['integ_mean'] * PPM:+.1f}",
                f"{s['out_rms'] * PPM:.0f}",
                f"{s['sigma_tau']:.4f}",
                f"{s['tau_span']:.3f}",
                f"[plot](symsync-measurements/{figure_of(s)})",
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
            "output mean, ppm",
            "integrator mean, ppm",
            "output rms, ppm",
            "timing error rms, symbols",
            "timing error span, symbols",
            "figure",
        ],
        rows,
    )


def stamp(scenarios: list[dict[str, Any]], meta: dict[str, Any]) -> str:
    """The run: when, where, at which commit and toolchain."""
    return table(
        ["run", "when", "machine", "commit", "toolchain", "command"],
        [
            [
                "`validate_timing_loop_noise --trace`, "
                f"{len(scenarios)} scenarios of "
                f"{scenarios[0]['symbols']} symbols, one seed",
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
