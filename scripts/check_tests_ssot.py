#!/usr/bin/env python3
"""Fail when a C test re-defines something `dp_test.h` already provides.

This directory held **90 copies of `CHECK` in six incompatible variants** --
two arities, two failure semantics, and one whose condition was inverted --
while `dp_state_test.h` documented the macro as "already present in every
test_*_core.c". Twenty of those copies had their failure gate drift above
later assertions, so 75 checks printed FAIL and their tests still exited 0.

None of that was a knowledge problem. `dp_state_test.h` said what it needed
and the convention was written down; the copies accumulated anyway. So the
rule is a gate, not a paragraph.

What it forbids
---------------
1. Re-defining any macro `dp_test.h` defines (`DP_CHECK`, `DP_TEST_END`, ...).
2. Defining an assertion macro of your own -- anything named `CHECK`,
   `REQUIRE`, `EXPECT` or `ASSERT`, with or without a suffix. That is the
   original sin, and it is what a new file scaffolded from an old template
   will do.
3. Re-implementing a comparison `dp_test.h` exports (`dp_nearf`, `dp_cnear`,
   ...), including under the historical names the consolidation retired.

The forbidden list is DERIVED from dp_test.h on every run, not written here,
so a macro added there is covered without editing this file.

Usage
-----
    python scripts/check_tests_ssot.py     # exit 1 on any violation
"""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TESTS = ROOT / "native" / "tests"
HEADER = TESTS / "dp_test.h"

# Names the consolidation retired. Kept as a short explicit list because they
# no longer exist anywhere to be derived FROM -- that is the whole point.
RETIRED = [
    "_almost_eq",
    "_almost_eq_c",
    "almost_eq_c",
    "_feq",
    "_ceq",
    "ceq",
    "ALMOST_EQ",
    "ALMOST_EQ_C",
    "FEQC",
]

ASSERTION_LIKE = re.compile(r"^(CHECK|REQUIRE|EXPECT|ASSERT)\w*$")


def provided() -> tuple[set[str], set[str]]:
    """(macros, functions) that dp_test.h defines."""
    text = HEADER.read_text()
    macros = set(re.findall(r"^#define\s+(\w+)", text, re.M))
    funcs = set(re.findall(r"^(dp_\w+)\s*\(", text, re.M))
    return macros, funcs


def main() -> int:
    if not HEADER.exists():
        print(f"check_tests_ssot: {HEADER} is missing — nothing to enforce,")
        print("  so this gate has not run, so it has not passed.")
        return 1

    macros, funcs = provided()
    if not macros:
        print("check_tests_ssot: parsed NO macros out of dp_test.h —")
        print("  the scan found nothing, so it did not run.")
        return 1

    bad: list[str] = []
    for path in sorted(TESTS.glob("*.c")) + sorted(TESTS.glob("*.h")):
        if path.name == HEADER.name:
            continue
        for n, line in enumerate(path.read_text().splitlines(), 1):
            m = re.match(r"\s*#\s*define\s+(\w+)", line)
            if m:
                name = m.group(1)
                if name in macros:
                    bad.append(
                        f"{path.relative_to(ROOT)}:{n}: re-defines "
                        f"{name}, which dp_test.h provides"
                    )
                elif ASSERTION_LIKE.match(name):
                    bad.append(
                        f"{path.relative_to(ROOT)}:{n}: defines its own "
                        f"assertion {name} — use DP_CHECK / DP_REQUIRE"
                    )
                elif name in RETIRED:
                    bad.append(
                        f"{path.relative_to(ROOT)}:{n}: {name} was "
                        f"retired — use dp_nearf / dp_cnearf"
                    )
            m = re.match(
                r"\s*(?:static\s+)?(?:inline\s+)?\w[\w \t*]*?(\w+)\s*\($",
                line.rstrip(),
            )
            fn = re.match(r"^(\w+)\s*\(", line)
            for cand in (
                m.group(1) if m else None,
                fn.group(1) if fn else None,
            ):
                if cand and (cand in funcs or cand in RETIRED):
                    bad.append(
                        f"{path.relative_to(ROOT)}:{n}: re-implements "
                        f"{cand} — dp_test.h exports it"
                    )

    if bad:
        print("check_tests_ssot: the shared harness is the single definition.")
        for b in bad:
            print(f"  {b}")
        print(f"\n  {len(bad)} violation(s). See native/tests/README.md.")
        return 1

    scanned = len(list(TESTS.glob("*.c")))
    print(
        f"check_tests_ssot: OK — {scanned} tests, "
        f"{len(macros)} macros and {len(funcs)} helpers owned by dp_test.h"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
