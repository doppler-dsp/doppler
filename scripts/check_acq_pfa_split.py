#!/usr/bin/env python3
"""Gate: every gate the acquisition engine prices splits over ONE cell count.

A dwell's statistic is the largest cell of the Doppler-INTERPOLATED surface,
so the per-cell false-alarm probability must be the Sidak share of `pfa` over
the cells that maximum ranges over. `acq_band_cells()` in
`native/src/acq/acq_core.c` is that count: every searched row of the
interpolated surface, times `code_bins`. Pricing a gate over a hand count of
the native cells instead (`searched_bins * code_bins`) fired every D > 1
engine at about twice its `pfa` (doppler#2218).

Three sites price a gate: acq_commit_thresholds(), acq_ascend_n_noncoh() and
the burst sizer's depth loop. Only the first has an exact pin
(test_acq_core.c's _acq_pfa_split_check reads the committed pfa_cell), so a
hand count re-inlined at either of the other two would pass every test. This
holds all of them: each `dp_det_pfa_cell (` call under `native/src/acq/` must
take `acq_band_cells (` in its arguments. A tree with no such call fails too,
because then this gate is checking nothing.

Usage:  python3 scripts/check_acq_pfa_split.py [--root DIR]
Exit 0 when every call takes the one count; 1 when one does not; 2 when
there is no call to check.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCAN_DIR = "native/src/acq"
CALL = re.compile(r"(?<![\w.])dp_det_pfa_cell\s*\(")
COUNT = re.compile(r"(?<![\w.])acq_band_cells\s*\(")
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)


def _blank_comments(src: str) -> str:
    """Comments replaced by spaces, newlines kept, so offsets map to lines."""
    return COMMENT.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), src)


def _arguments(src: str, open_paren: int) -> str:
    """The text between `(` at `open_paren` and its matching `)`."""
    depth = 0
    for i in range(open_paren, len(src)):
        if src[i] == "(":
            depth += 1
        elif src[i] == ")":
            depth -= 1
            if depth == 0:
                return src[open_paren + 1 : i]
    return src[open_paren + 1 :]


def calls(root: Path) -> list[tuple[str, int, bool]]:
    """(file, line, takes the one count) for every call under SCAN_DIR."""
    out: list[tuple[str, int, bool]] = []
    for path in sorted((root / SCAN_DIR).rglob("*.c")):
        src = _blank_comments(path.read_text(encoding="utf-8"))
        for m in CALL.finditer(src):
            args = _arguments(src, m.end() - 1)
            line = src.count("\n", 0, m.start()) + 1
            rel = path.relative_to(root).as_posix()
            out.append((rel, line, bool(COUNT.search(args))))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", type=Path, default=ROOT)
    a = ap.parse_args()
    found = calls(a.root)
    if not found:
        print(
            f"acq-pfa-split: no dp_det_pfa_cell call under {SCAN_DIR}, so "
            "this gate holds nothing -- did the pricing move?"
        )
        return 2
    bad = [(f, n) for f, n, ok in found if not ok]
    if not bad:
        print(
            f"acq-pfa-split: all {len(found)} gate prices split over "
            "acq_band_cells()"
        )
        return 0
    print("acq-pfa-split: a gate is priced over a count of its own --")
    for f, n in bad:
        print(f"  {f}:{n}: dp_det_pfa_cell without acq_band_cells")
    print(
        "  Split pfa over acq_band_cells (D, sb, window_bins, code_bins),\n"
        "  times the rate hypotheses: the cells the dwell's maximum ranges\n"
        "  over, every row of the interpolated surface. A hand count of the\n"
        "  native cells fires the engine at about twice its pfa\n"
        "  (doppler#2218)."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
