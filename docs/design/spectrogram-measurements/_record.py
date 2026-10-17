"""What every rendered run in the Spectrogram's measurement record shares.

The record (``docs/design/spectrogram-measurements.md``) quotes runs of
``make bench-interleaved``. ``bench_interleaved.py`` keeps only each row's
best pass and deletes the per-pass snapshots with its worktrees, so each
run's passes were saved as they were written and are committed beside its
script, one directory per run with a ``<build>/`` directory per build. Each
run's script (``u1u4.py``, ``b2094.py``, ``b2062.py``) owns only its tables.
This module is the rest, once, so they cannot disagree on how a cell is
computed or checked:

- a row's cost in one pass is ``stats.min / stats.iterations``, in ns per
  the bench's unit (a sample for a Spectrogram row, a frame for PSD's
  kernel rows and AccTrace's fold);
- a cell is the median of that over the passes, portable / native;
- a spread is ``(max - min) / median`` over the same passes.

A run's tables sit between ``<!-- spectrogram-<run>:<block>:start -->`` and
``:end`` markers in the record. The marker blocks, ``--write`` and
``--check`` are ``../_record_blocks.py``'s, the one home every rendered
record in ``docs/design/`` shares.
"""

from __future__ import annotations

import json
import statistics
import sys
from pathlib import Path
from typing import TYPE_CHECKING

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import _record_blocks as blocks
from _record_blocks import table

if TYPE_CHECKING:
    from collections.abc import Callable

__all__ = ["table"]

RECORD = Path(__file__).resolve().parent.parent / "spectrogram-measurements.md"
BUILDS = ("portable", "native")
PASSES = 5

Pass = dict[str, float]
Runs = dict[str, list[Pass]]


def load(data: Path, build: str, commit: str) -> list[Pass]:
    """Each pass of one build: row name to cost per unit, in ns.

    Every snapshot must have been measured at ``commit`` on a clean tree
    (its ``commit_info``), so a pass from another run cannot be dropped in
    and still render: the table would be the right shape and the wrong run.
    """
    passes = []
    for f in sorted((data / build).glob("*-c.json")):
        snap = json.loads(f.read_text(encoding="utf-8"))
        info = snap["commit_info"]
        if not info["id"].startswith(commit) or info["dirty"]:
            raise SystemExit(
                f"{f.name}: measured at {info['id'][:9]} "
                f"(dirty={info['dirty']}), not a clean {commit}"
            )
        rows = snap["benchmarks"]
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


def runs(data: Path, commit: str, meta: bool = True) -> Runs:
    """Every pass of both builds of one run, held to its commit and count.

    The run's merged ``doppler_meta`` is committed beside its passes
    (``meta.json``, one block per build), and each block must name the same
    commit: that file is what the record's machine and governor claims
    cite, and the merged set it came from is not kept. ``meta=False`` is
    for the one run that predates it (u1u4, whose ``doppler_meta`` was
    never committed); for every other run a missing file is refused, so
    deleting it cannot leave the record citing nothing.
    """
    P = {b: load(data, b, commit) for b in BUILDS}
    if any(len(P[b]) != PASSES for b in BUILDS):
        raise SystemExit(f"expected {PASSES} passes per build in {data}")
    path = data / "meta.json"
    if meta and not path.exists():
        raise SystemExit(f"{path}: missing -- the run's doppler_meta")
    if path.exists():
        m = json.loads(path.read_text(encoding="utf-8"))
        bad = [b for b in BUILDS if m[b]["commit"] != commit]
        if bad:
            raise SystemExit(f"{path}: {bad} not measured at {commit}")
    return P


def main(
    run: str,
    data: Path,
    commit: str,
    blocks_by_name: dict[str, Callable[[Runs], str]],
    doc: str,
    meta: bool = True,
) -> int:
    """Print, ``--write`` or ``--check`` one run's tables in the record."""

    def render() -> dict[str, str]:
        P = runs(data, commit, meta)
        return {name: f(P) for name, f in blocks_by_name.items()}

    return blocks.main(
        RECORD,
        f"spectrogram-{run}",
        run,
        f"docs/design/spectrogram-measurements/{run}.py",
        render,
        doc,
    )
