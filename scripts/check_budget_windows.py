#!/usr/bin/env python3
"""Gate: no receiver measurement opens its window on an analytic budget alone.

A receiver's error rate is scored from the moment it is observed locked:
every lock flag it publishes sustained, inside a stated lock allocation,
aligned once there (`dp_ber_lock_all()`, native/tests/dp_ber_test.h;
docs/dev/contributing/measuring-a-receiver.md, "The traps"). Two older paths
score from somewhere else:

- **the analytic budget**: `dp_ber_settle()` and `dp_ber_settle_from()` in C,
  `settle_from()` and `ber_settle_from()` in Python, take the later of
  `2 (5/bn_t + 5/bn_c)` and whichever lock flags the caller passed. With
  NULL, or one flag of several, they score a loop that is still pulling in:
  one async-receiver draw read 13,948 errors where the observed-lock window
  reads 27 (#3231);
- **the best-lag search**: `_best_ber()` scores the minimum over lags and
  rotations, an optimisation over the answer rather than a measurement
  (#3080).

Every call of either path, in code (comments and strings do not count), is
counted per file and held to `scripts/.budget-window-exempt`, which only
shrinks: a file over its listed count, or a file with calls and no row, is
red; a row above its file's count is red too, so a removed call takes its
row down with it. The definitions are not calls and are not scanned
(DEFINING below).

Usage:  python3 scripts/check_budget_windows.py [--root DIR]
Exit 0 when every file is at its row; 1 otherwise.
"""

from __future__ import annotations

import argparse
import io
import re
import subprocess
import sys
import tokenize
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LIST = "scripts/.budget-window-exempt"
#: Where the paths are defined: their own definitions and bindings.
DEFINING = (
    "native/tests/dp_ber_test.h",
    "native/inc/doppler/ber/",
    "native/src/ber/",
)
C_CALL = re.compile(
    r"(?<![\w.>])(dp_ber_settle|dp_ber_settle_from|_best_ber)\s*\("
)
PY_CALL = re.compile(r"(?<![\w])(settle_from|ber_settle_from|_best_ber)\s*\(")
C_COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*|\"(?:\\.|[^\"\\\n])*\"", re.S)
#: A C definition: the name at the start of a line, the GNU style's
#: function-definition layout (`static inline size_t\ndp_ber_settle (`).
C_DEF = re.compile(r"^(dp_ber_settle|dp_ber_settle_from|_best_ber)\s*\(", re.M)


def _c_code(src: str) -> str:
    """Comments and string literals blanked, definitions removed."""
    src = C_COMMENT.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), src)
    return C_DEF.sub(lambda m: " " * len(m.group(0)), src)


def _py_code(src: str) -> str:
    """The source with comments and strings blanked and `def` names gone."""
    out: list[str] = []
    try:
        toks = list(tokenize.generate_tokens(io.StringIO(src).readline))
    except (tokenize.TokenError, SyntaxError):
        return src
    prev = ""
    for t in toks:
        if t.type in (tokenize.COMMENT, tokenize.STRING):
            continue
        if t.type == tokenize.NAME and prev == "def":
            prev = t.string
            continue
        out.append(t.string)
        prev = t.string
    return " ".join(out)


def count(root: Path) -> dict[str, int]:
    """Calls a file, for every tracked C or Python file that has one."""
    files = subprocess.run(
        ["git", "ls-files", "native", "src"],
        cwd=root,
        capture_output=True,
        text=True,
        check=False,
    ).stdout.split()
    if not files:  # a seeded tree with no git: walk it
        files = [
            str(p.relative_to(root))
            for d in ("native", "src")
            if (root / d).is_dir()
            for p in (root / d).rglob("*")
            if p.is_file()
        ]
    got: dict[str, int] = {}
    for f in sorted(files):
        if f.startswith(DEFINING) or not f.endswith((".c", ".h", ".py")):
            continue
        src = (root / f).read_text(encoding="utf-8", errors="replace")
        if f.endswith(".py"):
            n = len(PY_CALL.findall(_py_code(src)))
        else:
            n = len(C_CALL.findall(_c_code(src)))
        if n:
            got[f] = n
    return got


def listed(root: Path) -> dict[str, int]:
    """The list's rows: `path count reason`."""
    rows: dict[str, int] = {}
    path = root / LIST
    if not path.exists():
        return rows
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        f = line.split()
        rows[f[0]] = int(f[1])
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", type=Path, default=ROOT)
    root = ap.parse_args().root
    got, rows = count(root), listed(root)
    bad = []
    for f in sorted(set(got) | set(rows)):
        n, cap = got.get(f, 0), rows.get(f)
        if cap is None:
            bad.append(
                f"{f}: {n} budget-window call(s) and no row in {LIST}: "
                "open the window with dp_ber_lock_all() "
                "(measuring-a-receiver.md)"
            )
        elif n > cap:
            bad.append(f"{f}: {n} call(s), over its row's {cap}")
        elif n < cap:
            bad.append(
                f"{f}: {n} call(s) under its row's {cap}: shrink the row"
            )
    for b in bad:
        print(b)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
