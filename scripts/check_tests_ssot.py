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

The assertion ratchet
---------------------
The second half of this gate exists because naming was not the only way the
harness lost ground. A migration, a rebase, or a badly resolved conflict can
silently REDUCE the number of assertions in a file: it still compiles, the
survivors still pass, `ctest` still reports 100%, and nothing announces that
coverage went backwards. That is a smaller-denominator failure -- read the
count, not the percentage.

It is not hypothetical either. This branch was cut before
`feat(telemetry)` landed on `main`, and the consolidation would have dropped
**43 assertions across three files** (`test_RateConverter_core.c` -20,
`test_mpsk_receiver_core.c` -12, `test_agc_core.c` -11) with a green suite.
One of the three was spotted by review; the other two only by counting.

So: no test file may end up with fewer assertions than the base ref has,
unless it is listed in `native/tests/.assertion-ratchet-ignore` with a
reason. Deliberate removals are fine and rare; silent ones are the bug.

Usage
-----
    python scripts/check_tests_ssot.py                  # naming only
    python scripts/check_tests_ssot.py --base origin/main   # + the ratchet
"""

from __future__ import annotations

import pathlib
import re
import subprocess
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


IGNORE = TESTS / ".assertion-ratchet-ignore"

# What counts as an assertion, on either side of the migration. Both sets are
# counted the same way -- per line, skipping `#define` -- so the comparison is
# between call sites, never between a definition and a call.
OLD_ASSERT = re.compile(r"(?<![A-Za-z0-9_])CHECK\w*\s*\(")
NEW_ASSERT = re.compile(
    r"(?<![A-Za-z0-9_])(DP_CHECK|DP_REQUIRE|DP_CHECK_MSG|DP_REQUIRE_MSG"
    r"|DP_CHECK_NEAR|DP_RECORD_FAIL|DP_STATE_ROUNDTRIP_TEST)\s*\("
)


def count_assertions(text: str) -> int:
    n = 0
    for line in text.splitlines():
        if re.match(r"\s*#\s*define", line):
            continue
        n += len(OLD_ASSERT.findall(line)) + len(NEW_ASSERT.findall(line))
    return n


def ratchet(base: str) -> list[str]:
    """Fail any test file that has fewer assertions than `base` has."""
    rev = subprocess.run(
        ["git", "rev-parse", "--verify", f"{base}^{{commit}}"],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if rev.returncode != 0:
        raise LookupError(
            f"base ref {base!r} does not resolve.\n"
            "  A ratchet that cannot read its baseline has not passed, so\n"
            "  this is a failure rather than a skip — but it is NOT a lost\n"
            "  assertion, and reporting it as one sends the reader hunting\n"
            "  a regression that is not there.\n"
            "  Fetch it:  git fetch --no-tags --depth=1 origin \\\n"
            "               +refs/heads/main:refs/remotes/origin/main"
        )

    excused = {}
    if IGNORE.exists():
        for line in IGNORE.read_text().splitlines():
            line = line.split("#", 1)[0].strip()
            if line:
                name, _, why = line.partition(" ")
                excused[name] = why.strip()

    listing = subprocess.run(
        ["git", "ls-tree", "-r", "--name-only", base, "native/tests/"],
        cwd=ROOT,
        capture_output=True,
        text=True,
    ).stdout.split()

    bad = []
    for rel in (f for f in listing if f.endswith(".c")):
        was = subprocess.run(
            ["git", "show", f"{base}:{rel}"],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if was.returncode != 0:
            continue
        now = ROOT / rel
        if not now.exists():
            continue  # deletion is a visible act, not a silent loss
        before, after = (
            count_assertions(was.stdout),
            count_assertions(now.read_text()),
        )
        if after < before:
            key = pathlib.Path(rel).name
            if key in excused:
                continue
            bad.append(
                f"{rel}: {before} assertions at {base}, {after} here "
                f"(-{before - after})"
            )
    return bad


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

    base = None
    for i, a in enumerate(sys.argv):
        if a == "--base" and i + 1 < len(sys.argv):
            base = sys.argv[i + 1]
    try:
        lost = ratchet(base) if base else []
    except LookupError as exc:
        print(f"check_tests_ssot: {exc}")
        return 1
    if lost:
        print("check_tests_ssot: a test file LOST assertions.")
        for line in lost:
            print(f"  {line}")
        print("\n  A suite can go green while covering less. If a removal is")
        print("  deliberate, list the file in")
        print(f"  {IGNORE.relative_to(ROOT)} with the reason.")
        return 1

    scanned = len(list(TESTS.glob("*.c")))
    print(
        f"check_tests_ssot: OK — {scanned} tests, "
        f"{len(macros)} macros and {len(funcs)} helpers owned by dp_test.h"
        + (f"; no file lost assertions vs {base}" if base else "")
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
