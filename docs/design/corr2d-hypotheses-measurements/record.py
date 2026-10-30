"""Render the hypothesis bank's timing tables from its harness's run.

``docs/design/corr2d-hypotheses.md`` §5 quotes one run of the harness
``native/validation/corr2d_hypothesis_order.c``, its ``--emit`` output kept
beside this file:

- ``timing.json``: for each code length ``nx``, grid factor ``L`` and worker
  count, the time of one whole frame (the minimum over the harness's repeats)
  through the full pipeline and through each ordered method, split by stage;
- ``meta.json``: the run's stamp (``_record_blocks.run_stamp``): when,
  where, from which commit and with which toolchain.

Every table of §5 is this file's output, held between
``<!-- corr2d-hypotheses:<block>:start -->`` and ``:end`` markers, so a cell
cannot be edited by hand, or left behind by a re-run, without ``--check``
failing. Data from a dirty tree is refused.

Usage::

    python record.py measure     # the harness's --emit -> timing.json
    python record.py             # print the blocks
    python record.py --write     # rewrite them in the page
    python record.py --check     # fail when they differ (the gate)

``make corr2d-hypotheses-record`` builds, measures and writes; ``--check``
is a line of the Makefile's ``DOCS_CHECK_PRE_CMDS``.
"""

from __future__ import annotations

import json
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import _record_blocks as blocks
from _record_blocks import table

DATA = Path(__file__).resolve().parent
ROOT = DATA.parents[2]
PAGE = DATA.parent / "corr2d-hypotheses.md"
SCRIPT = "docs/design/corr2d-hypotheses-measurements/record.py"
SOURCE = ROOT / "native/validation/corr2d_hypothesis_order.c"
VALIDATOR = ROOT / "build/native/validation/validate_corr2d_hypothesis_order"


def measure() -> int:
    """Run the built harness's --emit; write timing.json and its stamp."""
    meta = blocks.run_stamp(ROOT, VALIDATOR, SOURCE)
    t0 = time.perf_counter()
    run = subprocess.run(
        [str(VALIDATOR), "--emit"], capture_output=True, text=True
    )
    if run.returncode != 0:
        sys.stderr.write(run.stderr)
        return run.returncode
    json.loads(run.stdout)  # refuse to keep a run that is not one document
    meta["load_after"] = blocks.load_avg()
    meta["wall_s"] = round(time.perf_counter() - t0, 1)
    (DATA / "timing.json").write_text(run.stdout, encoding="utf-8")
    (DATA / "meta.json").write_text(
        json.dumps(meta, indent=1) + "\n", encoding="utf-8"
    )
    print(f"measure: {VALIDATOR.name} --emit in {meta['wall_s']} s")
    return 0


def load() -> tuple[dict[str, Any], dict[str, Any]]:
    """The run and its stamp, held to a clean tree and a full toolchain."""
    meta = json.loads((DATA / "meta.json").read_text(encoding="utf-8"))
    run = json.loads((DATA / "timing.json").read_text(encoding="utf-8"))
    blocks.require_toolchain("meta.json", meta)
    if meta["dirty"]:
        raise SystemExit("record data from a dirty tree is refused")
    return run, meta


Key = tuple[int, int, int]


def _by(run: dict[str, Any]) -> dict[Key, dict[str, dict[str, Any]]]:
    """Rows keyed by (nx, L, workers), then by method."""
    out: dict[Key, dict[str, dict[str, Any]]] = {}
    for r in run["rows"]:
        k = (r["nx"], r["L"], r["workers"])
        out.setdefault(k, {})[r["method"]] = r
    return out


def _ms(v: float) -> str:
    return f"{v:.2f}"


def serial(run: dict[str, Any], meta: dict[str, Any]) -> str:
    """One thread: each method's frame time, and what ordering buys."""
    g = run["geometry"]
    rows = []
    for (nx, lf, w), m in sorted(_by(run).items()):
        if w != 1:
            continue
        full, pad, mix = (m[k]["frame_ms"] for k in ("full", "padded", "mix"))
        best = min(pad, mix)
        rows.append(
            [
                f"{nx}",
                f"{lf}",
                f"{lf * g['rates']}",
                _ms(full),
                _ms(pad),
                _ms(mix),
                f"{full / best:.1f}x",
                f"{100 * (mix / pad - 1):+.0f}%",
                f"{100 * m['mix']['col_ms'] / mix:.0f}%",
            ]
        )
    return table(
        [
            "$n_x$",
            "$L$",
            "maps",
            "full (ms)",
            "zero-pad then roll (ms)",
            "mix per fraction (ms)",
            "ordering buys",
            "mix vs zero-pad",
            "slow-time share (mix)",
        ],
        rows,
    )


def pool(run: dict[str, Any], meta: dict[str, Any]) -> str:
    """On a pool: each method's frame time against its own serial time."""
    by = _by(run)
    rows = []
    for (nx, lf, w), m in sorted(by.items()):
        if w == 1:
            continue
        one = by[(nx, lf, 1)]
        cells = [f"{nx}", f"{lf}", f"{w}"]
        for k in ("full", "padded", "mix"):
            t = m[k]["frame_ms"]
            cells.append(f"{_ms(t)} ({t / one[k]['frame_ms']:.2f}x)")
        rows.append(cells)
    return table(
        [
            "$n_x$",
            "$L$",
            "workers",
            "full, ms (vs serial)",
            "zero-pad then roll, ms (vs serial)",
            "mix per fraction, ms (vs serial)",
        ],
        rows,
    )


def stamp(run: dict[str, Any], meta: dict[str, Any]) -> str:
    """The run: when, where, at which commit, toolchain and geometry."""
    g = run["geometry"]
    return table(
        [
            "run",
            "when",
            "machine",
            "commit",
            "toolchain",
            "geometry",
            "command",
        ],
        [
            [
                "`validate_corr2d_hypothesis_order --emit`",
                f"{meta['date_utc']} UTC, {meta['wall_s']:.0f} s",
                f"`{meta['machine']}`: {meta['cpu_model']} "
                f"({meta['cpu_threads']} threads)",
                f"`{meta['commit']}`, clean",
                blocks.tc_cell(meta),
                f"$n_y = {g['ny']}$, $n_{{y,\\text{{out}}}} = {g['ny_out']}$, "
                f"{g['rates']} rates x $L$ carriers, best of "
                f"{g['repeats']}",
                "`make corr2d-hypotheses-record`",
            ]
        ],
    )


BLOCKS = {"serial": serial, "pool": pool, "stamp": stamp}


def render() -> dict[str, str]:
    data = load()
    return {name: f(*data) for name, f in BLOCKS.items()}


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "measure":
        sys.exit(measure())
    sys.exit(
        blocks.main(
            PAGE,
            "corr2d-hypotheses",
            "corr2d-hypotheses timing",
            SCRIPT,
            render,
            __doc__ or "",
        )
    )
