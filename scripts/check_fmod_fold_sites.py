#!/usr/bin/env python3
"""Gate: folding a value into `[0, m)` has ONE home, `dp_fmod_pos()`.

C's `fmod()` keeps the dividend's sign, so every caller that wants a value
on a periodic axis -- a code phase in chips, a frequency in a band, a table
index -- has to add the period back when the result is negative. That
fix-up had been written by hand five times in this library before
`native/inc/clib_common.h` gave it one home (doppler#1249). A private copy
is where the next drift lives: a bound spelled `<=` in one and `<` in
another, a wrap that lands ON the period and indexes one past the end.

The signature of a private copy is a `fmod(` call followed, within a few
lines, by a sign test on its result (`< 0.0`, `< 0)`) -- or `fmod` applied
to `fmod`, the branchless spelling of the same thing. Library C only
(`native/inc`, `native/src`); tests and harnesses are oracles and may spell
arithmetic as they like. No allowlist: every copy was converted when the
gate landed, so the first occurrence fails.

Usage:  python3 scripts/check_fmod_fold_sites.py
Exit 0 when no private fold exists.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SANCTIONED = "native/inc/clib_common.h"
SCAN_DIRS = ("native/inc", "native/src")
FMOD = re.compile(r"\bfmodf?\s*\(")
SIGN_FIX = re.compile(r"<\s*0(\.0f?)?\s*\)")
NESTED = re.compile(r"\bfmodf?\s*\(\s*fmodf?\s*\(")
WINDOW = 3  # lines after the fmod call in which a sign test is its fix-up


def private_folds() -> list[tuple[str, int, str]]:
    found: list[tuple[str, int, str]] = []
    for d in SCAN_DIRS:
        for path in sorted((ROOT / d).rglob("*")):
            if path.suffix not in (".c", ".h"):
                continue
            rel = path.relative_to(ROOT).as_posix()
            if rel == SANCTIONED or "_ext" in path.name:
                continue
            lines = path.read_text(errors="replace").splitlines()
            for i, line in enumerate(lines):
                if not FMOD.search(line):
                    continue
                if NESTED.search(line):
                    found.append((rel, i + 1, line.strip()))
                    continue
                tail = lines[i + 1 : i + 1 + WINDOW]
                if any(SIGN_FIX.search(t) for t in tail):
                    found.append((rel, i + 1, line.strip()))
    return found


def main() -> int:
    found = private_folds()
    if not found:
        print("fmod-fold: one home (dp_fmod_pos), no private copy")
        return 0
    print("fmod-fold: a fold into [0, m) is written by hand here --")
    for rel, n, line in found:
        print(f"  {rel}:{n}: {line}")
    print(
        "  Call dp_fmod_pos(x, m) (native/inc/clib_common.h) instead. If the\n"
        "  sign test after this fmod() is NOT a fold (a genuinely signed\n"
        "  remainder), move the test further than 3 lines from the call or\n"
        "  compute the remainder without fmod; the gate cannot tell them\n"
        "  apart, and the ambiguity is the reader's problem too."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
