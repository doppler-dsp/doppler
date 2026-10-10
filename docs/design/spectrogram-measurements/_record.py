"""What every rendered run in the Spectrogram's measurement record shares.

The record (``docs/design/spectrogram-measurements.md``) quotes runs of
``make bench-interleaved``. ``bench_interleaved.py`` keeps only each row's
best pass and deletes the per-pass snapshots with its worktrees, so each
run's passes were saved as they were written and are committed beside its
script, one directory per run with a ``<build>/`` directory per build. Each
run's script (``u1u4.py``, ``b2094.py``) owns only its tables. This module
is the rest, once, so the two cannot disagree on how a cell is computed or
checked:

- a row's cost in one pass is ``stats.min / stats.iterations``, in ns per
  the bench's unit (a sample for a Spectrogram row, a frame for PSD's
  kernel rows and AccTrace's fold);
- a cell is the median of that over the passes, portable / native;
- a spread is ``(max - min) / median`` over the same passes.

A run's tables sit between ``<!-- spectrogram-<run>:<block>:start -->`` and
``:end`` markers in the record. ``--write`` rewrites them, and ``--check``
(in ``make docs-invariants``) fails when they differ from what the data
renders, cell by cell, so mdformat's column padding is not a difference.
"""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from collections.abc import Callable

RECORD = Path(__file__).resolve().parent.parent / "spectrogram-measurements.md"
BUILDS = ("portable", "native")
PASSES = 5

Pass = dict[str, float]
Runs = dict[str, list[Pass]]


def load(data: Path, build: str) -> list[Pass]:
    """Each pass of one build: row name to cost per unit, in ns."""
    passes = []
    for f in sorted((data / build).glob("*-c.json")):
        rows = json.loads(f.read_text(encoding="utf-8"))["benchmarks"]
        passes.append(
            {
                r["name"]: r["stats"]["min"] / r["stats"]["iterations"] * 1e9
                for r in rows
            }
        )
    return passes


def med(passes: list[Pass], f: Callable[[Pass], float]) -> float:
    return statistics.median(f(p) for p in passes)


def spread(passes: list[Pass], f: Callable[[Pass], float]) -> float:
    v = [f(p) for p in passes]
    return (max(v) - min(v)) / statistics.median(v)


def both(P: Runs, f: Callable[[list[Pass]], float], fmt: str) -> str:
    return " / ".join(format(f(P[b]), fmt) for b in BUILDS)


def table(head: list[str], rows: list[list[str]]) -> str:
    out = ["| " + " | ".join(head) + " |"]
    out.append("| " + " | ".join("---" for _ in head) + " |")
    out += ["| " + " | ".join(r) + " |" for r in rows]
    return "\n".join(out)


def _span(run: str, name: str) -> re.Pattern[str]:
    return re.compile(
        rf"(<!-- spectrogram-{run}:{name}:start -->)(.*?)"
        rf"(<!-- spectrogram-{run}:{name}:end -->)",
        re.S,
    )


def _cells(text: str) -> list[list[str]]:
    """A block's table rows as stripped cells; padding and dashes ignored."""
    rows = []
    for line in text.strip().splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        rows.append([re.sub(r"^:?-+:?$", "---", c) for c in cells])
    return rows


def main(
    run: str,
    data: Path,
    blocks: dict[str, Callable[[Runs], str]],
    doc: str,
) -> int:
    """Print, ``--write`` or ``--check`` one run's tables in the record."""
    ap = argparse.ArgumentParser(description=doc.splitlines()[0])
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    a = ap.parse_args()
    P = {b: load(data, b) for b in BUILDS}
    if any(len(P[b]) != PASSES for b in BUILDS):
        raise SystemExit(
            f"{run}: expected {PASSES} passes per build in {data}"
        )
    rendered = {name: f(P) for name, f in blocks.items()}
    if not (a.write or a.check):
        for name, body in rendered.items():
            print(f"== {name}\n{body}\n")
        return 0
    text = RECORD.read_text(encoding="utf-8")
    stale = []
    for name, body in rendered.items():
        m = _span(run, name).search(text)
        if m is None:
            raise SystemExit(f"{run}: no {name} markers in {RECORD.name}")
        if _cells(m.group(2)) != _cells(body):
            stale.append(name)
        text = _span(run, name).sub(
            lambda m, b=body: m.group(1) + "\n" + b + "\n" + m.group(3), text
        )
    script = f"docs/design/spectrogram-measurements/{run}.py"
    if a.write:
        RECORD.write_text(text, encoding="utf-8")
        print(f"{run}: wrote {len(rendered)} table block(s) in {RECORD.name}")
        return 0
    if stale:
        print(
            f"{run}: {RECORD.name}'s {', '.join(stale)} table(s) are not "
            f"what the run's data renders. Run `python {script} --write`; "
            f"a cell is the data's output, never an edit."
        )
        return 1
    print(f"{run}: OK — {len(rendered)} table block(s) match the run's data")
    return 0
