"""Render the acquisition surface's §8 tables from its validator's run.

The design page (``docs/design/acq-surface.md`` §8) quotes one run of the
shipped validator ``native/validation/acq_surface.c``, its ``--emit``
output kept beside this file:

- ``surface.json``: every section the validator computes, from the code's
  ambiguity function (the off-lag sidelobes at a tile offset, at the
  page's emitters and swept over the band), what a window on either
  transform would cost and buy, and what the shipped engine's peak lists
  hold over the windowed stream and over data;
- ``meta.json``: the run's stamp (``_record_blocks.run_stamp``): when,
  where, from which commit and with which toolchain.

Every table of §8 is this file's output. The page holds each between
``<!-- acq-surface:<block>:start -->`` and ``:end`` markers, so a cell
cannot be edited by hand, or left behind by a re-run, without ``--check``
failing. The blocks and the ``--write``/``--check`` machinery are
``../_record_blocks.py``'s. Data from a dirty tree is refused.

Usage::

    python record.py measure     # the validator's --emit -> surface.json
    python record.py             # print the blocks
    python record.py --write     # rewrite them in the page
    python record.py --check     # fail when they differ (the gate)

``make acq-surface-record`` builds the validator, measures and writes;
``--check`` is a line of the Makefile's ``DOCS_CHECK_PRE_CMDS``.
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
PAGE = DATA.parent / "acq-surface.md"
SCRIPT = "docs/design/acq-surface-measurements/record.py"
SOURCE = ROOT / "native/validation/acq_surface.c"
VALIDATOR = ROOT / "build/native/validation/validate_acq_surface"

#: How each window is named on the page.
WINDOW = {
    "none": "none (shipped)",
    "hann": "Hann",
    "hamming": "Hamming",
    "blackman": "Blackman",
}


def measure() -> int:
    """Run the built validator's --emit; write surface.json and its stamp."""
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
    (DATA / "surface.json").write_text(run.stdout, encoding="utf-8")
    (DATA / "meta.json").write_text(
        json.dumps(meta, indent=1) + "\n", encoding="utf-8"
    )
    print(f"measure: {VALIDATOR.name} --emit in {meta['wall_s']} s")
    return 0


def load() -> tuple[dict[str, Any], dict[str, Any]]:
    """The run and its stamp, held to a clean tree and a full toolchain."""
    meta = json.loads((DATA / "meta.json").read_text(encoding="utf-8"))
    run = json.loads((DATA / "surface.json").read_text(encoding="utf-8"))
    blocks.require_toolchain("meta.json", meta)
    if meta["dirty"]:
        raise SystemExit("record data from a dirty tree is refused")
    return run, meta


def _m(v: float, fmt: str = ".2f") -> str:
    """A signed value as page math: `$-22.02$`. Computed values print two
    places; a measured one passes `.1f`, its spread being wider."""
    return f"${v:{fmt}}$"


def _hz(v: float) -> str:
    return f"{v:.0f} Hz" if v >= 0 else f"${v:.0f}$ Hz"


def _tiles(t: list[int]) -> str:
    qs = [f"${q:+d}$" for q in t]
    if len(qs) == 1:
        return f"tile {qs[0]}"
    return f"tiles {' and '.join(qs)}, tied"


def _chips(c: float) -> str:
    return f"{c:.0f}" if c == int(c) else f"{c:.1f}"


def ambiguity(run: dict[str, Any], meta: dict[str, Any]) -> str:
    """The largest off-lag sidelobe at each of the page's emitters."""
    fe = run["geometry"]["f_e_hz"]
    rows = []
    for r in run["ambiguity"]:
        f = r["f_hz"]
        res = f - round(f / fe) * fe
        resid = (
            f"$\\pm {abs(res):.0f}$ Hz, the edge"
            if abs(abs(res) - fe / 2) < 1e-6
            else f"{abs(res):.0f} Hz"
        )
        rows.append(
            [
                _hz(f),
                resid,
                f"{_m(r['db'])} dB",
                f"{_tiles(r['tiles'])}, {_chips(r['chips'])} chips off",
            ]
        )
    return table(
        [
            "emitter Doppler $f$",
            "from its tile's center",
            "largest off-lag sidelobe",
            "where",
        ],
        rows,
    )


def _worst(rows: list[dict[str, Any]]) -> dict[str, Any]:
    """The row whose off-lag peak came closest to the list's first."""
    return max(
        (r for r in rows if r["worst_db"] is not None),
        key=lambda r: r["worst_db"],
    )


def limits(run: dict[str, Any], meta: dict[str, Any]) -> str:
    """The near-far limits: the ambiguity's, and the lists' at each depth."""
    g, sw = run["geometry"], run["sweep"]
    by = {}
    for r in run["lists"]:
        by.setdefault(r["depth"], []).append(r)
    rows = [
        [
            f"the code's ambiguity, the emitter swept every "
            f"{g['sweep_hz']:.0f} Hz over $\\pm {g['uncertainty_hz']:.0f}$ Hz",
            f"{_m(sw['db'])} dB",
            f"at {_hz(sw['at_hz'])}",
        ]
    ]
    for d in sorted(by, reverse=True):
        w = _worst(by[d])
        hi = max(by[d], key=lambda r: r["cn0"])
        rows.append(
            [
                f"the engine's lists at $D = {d}$",
                f"{_m(w['worst_db'], '.1f')} dB",
                f"at {w['cn0']:.0f} dB-Hz, in {w['off_lists']} of "
                f"{w['lists']} lists; {_m(hi['worst_db'], '.1f')} dB at "
                f"{hi['cn0']:.0f} dB-Hz",
            ]
        )
    return table(
        [
            "what sets it",
            "closest an off-lag peak comes to the emitter's peak",
            "where",
        ],
        rows,
    )


def lists(run: dict[str, Any], meta: dict[str, Any]) -> str:
    """The largest off-lag peak in a list, per depth and C/N0."""
    depths = sorted({r["depth"] for r in run["lists"]})
    cell = {(r["depth"], r["cn0"]): r for r in run["lists"]}
    data = {r["cn0"]: r for r in run["data"]}
    deep = max(depths)
    rows = []
    for cn0 in sorted({r["cn0"] for r in run["lists"]}):
        row = [f"{cn0:.0f} dB-Hz"]
        for d in depths:
            r = cell[(d, cn0)]
            row.append(
                "none"
                if r["worst_db"] is None
                else f"{_m(r['worst_db'], '.1f')} dB, in {r['off_lists']} of "
                f"{r['lists']} lists"
            )
        dr = data.get(cn0)
        row.append(
            "—"
            if dr is None
            else (
                f"none in {dr['lists']} lists"
                if dr["off_lists"] == 0
                else f"in {dr['off_lists']} of {dr['lists']} lists"
            )
        )
        rows.append(row)
    return table(
        ["C/N0"]
        + [f"$D = {d}$" for d in depths]
        + [f"$D = {deep}$, data throughout"],
        rows,
    )


def fast(run: dict[str, Any], meta: dict[str, Any]) -> str:
    """A window on the epoch before the correlation."""
    hz = " / ".join(
        f"{f:.0f}" if f >= 0 else f"${f:.0f}$"
        for f in (r["f_hz"] for r in run["ambiguity"])
    )
    rows = []
    for i, w in enumerate(run["fast"]):
        unit = " dB" if i == 0 else ""
        nxt = "null" if w["next_db"] is None else _m(w["next_db"]) + unit
        side = " / ".join(_m(v) for v in w["side_db"])
        rows.append(
            [
                WINDOW[w["window"]],
                _m(w["edge_db"]) + unit,
                _m(w["avg_db"]) + unit,
                nxt,
                side + unit,
            ]
        )
    return table(
        [
            "fast-time window",
            "edge loss",
            "average loss",
            "next tile, $\\nu = f_E$",
            f"worst off-lag sidelobe ({hz} Hz)",
        ],
        rows,
    )


def slow(run: dict[str, Any], meta: dict[str, Any]) -> str:
    """A window on the block's D epochs before the slow-time transform."""
    rows = []
    for i, w in enumerate(run["slow"]):
        unit = " dB" if i == 0 else ""
        z = w["zone_bins"]
        rows.append(
            [
                WINDOW[w["window"]],
                _m(w["worst_db"]) + unit,
                _m(w["avg_db"]) + unit,
                _m(w["leak_db"]) + unit,
                f"$\\pm {z}$ native bin" if i == 0 else f"$\\pm {z}$",
            ]
        )
    return table(
        [
            "slow-time window",
            "worst loss",
            "average loss",
            "own-column leakage past the zone",
            "zone",
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
            "operating point",
            "command",
        ],
        [
            [
                "`validate_acq_surface --emit`",
                f"{meta['date_utc']} UTC, {meta['wall_s']:.0f} s",
                f"`{meta['machine']}`: {meta['cpu_model']} "
                f"({meta['cpu_threads']} threads)",
                f"`{meta['commit']}`, clean",
                blocks.tc_cell(meta),
                f"$N = {g['n']}$, $W = {g['window_bins']}$ at "
                f"$f_E = {g['f_e_hz']:.0f}$ Hz, $D = {g['coherent_bins']}$, "
                f"$I = {g['interp']}$",
                "`make acq-surface-record`",
            ]
        ],
    )


BLOCKS = {
    "ambiguity": ambiguity,
    "limits": limits,
    "lists": lists,
    "fast": fast,
    "slow": slow,
    "stamp": stamp,
}


def render() -> dict[str, str]:
    data = load()
    return {name: f(*data) for name, f in BLOCKS.items()}


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "measure":
        sys.exit(measure())
    sys.exit(
        blocks.main(
            PAGE,
            "acq-surface",
            "acq-surface",
            SCRIPT,
            render,
            __doc__ or "",
        )
    )
