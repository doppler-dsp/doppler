#!/usr/bin/env python3
"""Gate: `lgamma` has ONE home in library C, `dp_lgamma()`.

C's `lgamma()` writes the sign of Gamma into the global `signgam`, so two
threads calling it at once race on a variable neither reads. The library
met that the first time two receivers rebuilt their tracking chains on
different threads (doppler#1260): every chain sizes a detector threshold
through `marcum_q()`, and ThreadSanitizer stopped the test on `lgamma`.
`lgamma_r()` takes the sign by pointer and touches no global;
`dp_lgamma()` (declared in `native/inc/clib_common.h`, defined in
`native/src/detection/marcum_q.c`) wraps it once, and a private `lgamma`
anywhere else is where the next race lives. Library C only
(`native/inc`, `native/src`); tests and harnesses are single-threaded
oracles and may call what they like. No allowlist: every call was
converted when the gate landed, so the first bare one fails.

Usage:  python3 scripts/check_lgamma_sites.py
Exit 0 when no bare lgamma exists.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SANCTIONED = "native/src/detection/marcum_q.c"  # dp_lgamma's definition
SCAN_DIRS = ("native/inc", "native/src")
BARE = re.compile(r"(?<![\w.])lgammaf?\s*\(")
REENTRANT = re.compile(r"(?<![\w.])lgamma_r\s*\(")


def bare_calls() -> list[tuple[str, int, str]]:
    found: list[tuple[str, int, str]] = []
    for d in SCAN_DIRS:
        for path in sorted((ROOT / d).rglob("*")):
            if path.suffix not in (".c", ".h"):
                continue
            rel = path.relative_to(ROOT).as_posix()
            if "_ext" in path.name:
                continue
            lines = path.read_text(errors="replace").splitlines()
            for i, line in enumerate(lines):
                if line.lstrip().startswith(("*", "/*", "//")):
                    continue  # prose about lgamma is not a call
                if BARE.search(line) or (
                    REENTRANT.search(line) and rel != SANCTIONED
                ):
                    found.append((rel, i + 1, line.strip()))
    return found


def main() -> int:
    found = bare_calls()
    if not found:
        print("lgamma-reentrant: one home (dp_lgamma), no bare lgamma")
        return 0
    print("lgamma-reentrant: a bare lgamma is called here --")
    for rel, n, line in found:
        print(f"  {rel}:{n}: {line}")
    print(
        "  Call dp_lgamma(x) (native/inc/clib_common.h) instead: lgamma()\n"
        "  writes the global signgam and races across threads; the one\n"
        "  lgamma_r() is dp_lgamma's own, in native/src/detection/marcum_q.c."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
