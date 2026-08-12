#!/usr/bin/env python3
"""Fail when a C test re-defines something the shared harness provides.

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
1. Re-defining any macro the `dp_*.h` family defines (`DP_CHECK`,
   `DP_TEST_END`, `DP_STATE_ROUNDTRIP_TEST`, ...).
2. Defining an assertion macro of your own -- anything named `CHECK`,
   `REQUIRE`, `EXPECT` or `ASSERT`, with or without a suffix. That is the
   original sin, and it is what a new file scaffolded from an old template
   will do.
3. Re-implementing a helper the family exports (`dp_nearf`, `dp_cgauss`,
   `dp_dsss_capture`, ...), including under the historical names the
   consolidation retired.
4. Rolling a private random source: an inline xorshift step, a hand-written
   Box-Muller, or either of the two uniform mappings. `dp_rng_test.h` is the
   sanctioned home and the only file exempt.

The forbidden list is DERIVED from the family on every run, not written here,
so a macro added to any member -- or a whole new member -- is covered without
editing this file.

Why (4) is here and not in check_stimulus_sources.py
---------------------------------------------------
That gate looked at hand-rolled Gaussian noise and declined: it "hits 72
files, most of them legitimately", and a ratchet that large is noise, and a
noisy gate gets switched off. Correct across the Python layer and the
validation harnesses. Not correct HERE. After the dp_rng_test.h consolidation
the count in `native/tests` is zero, so the rule is absolute rather than a
ratchet, and an absolute rule is worth a gate.

The cost of not having had it: the xorshift step was written out twenty times
and `gauss` five, and the fifth was a half-finished edit -- `(void)u1`, then
two uniforms drawn one shift apart from a two-shift recurrence. It delivered
mean +0.056 and variance 1.115 where it claimed N(0,1), so the file's only
AWGN test ran 0.47 dB hot on biased, heavy-tailed noise for as long as it
existed. Nothing failed. A private generator cannot be wrong in a way
anything notices, which is exactly why it may not be private.

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


def family() -> list[pathlib.Path]:
    """Every shared harness header, which is every `dp_*.h` here.

    Derived by glob rather than listed, for the same reason the forbidden
    macro set is derived from the headers: a new family member is covered the
    day it lands, without an edit here that someone has to remember.

    The glob is `dp_*.h` and NOT `dp_*_test.h`, which is the obvious spelling
    and is wrong: `dp_test.h` -- the founding member, and the one that owns
    every assertion -- does not match it. Written that way, this gate parsed
    six headers, reported a cheerful count of them, and quietly stopped
    noticing a test that re-defined `DP_CHECK`. Caught by sabotage; it read as
    correct. Hence the assertion in `main`.
    """
    return sorted(TESTS.glob("dp_*.h"))


def provided() -> tuple[dict[str, str], dict[str, str]]:
    """(macros, functions) the family defines, each mapped to its owner.

    This read `dp_test.h` alone until `dp_rng_test.h` landed, which meant the
    gate covered the assertions and nothing else: a test could re-implement
    `dp_cgauss` or re-declare `DP_STATE_ROUNDTRIP_TEST` and the checker had
    no opinion. Owning the name is the point of the family, not a property of
    one member of it.
    """
    macros: dict[str, str] = {}
    funcs: dict[str, str] = {}
    for header in family():
        text = header.read_text()
        for name in re.findall(r"^#define\s+(\w+)", text, re.M):
            macros.setdefault(name, header.name)
        for name in re.findall(r"^(dp_\w+)\s*\(", text, re.M):
            funcs.setdefault(name, header.name)
    return macros, funcs


#: Generator idioms that must not reappear in a test. `dp_rng_test.h` is the
#: sanctioned home and is exempt; everything else in `native/tests` gets its
#: randomness from there.
#:
#: This is the narrow version of a check `check_stimulus_sources.py`
#: deliberately declined to make. That gate says a hand-rolled Box-Muller
#: "hits 72 files, most of them legitimately", and a ratchet that large is
#: noise. True across the Python layer and the validation harnesses — and not
#: true HERE, where after the consolidation the count is zero and the rule is
#: absolute. A gate is only worth having where it can be a rule.
GENERATOR_IDIOMS = [
    (
        # The defining shape is that the SAME lvalue appears on both sides:
        # `x ^= x << 13`, `*st ^= *st >> 17`. Hence the backreference, and it
        # is load-bearing in both directions. Without the `*` form the pattern
        # missed nine of the twenty historical copies while looking like it
        # worked; with `*` but without the backreference it matched
        # `test_burst_demod_core.c`'s CRC-16 (`c ^= (bits[i] & 1u) << 15`),
        # which xors a DIFFERENT expression and is not a generator at all.
        # Both mistakes were made here, and both were caught by sabotage
        # rather than by reading.
        re.compile(r"(\*?\w+(?:\[\w+\])?)\s*\^=\s*\1\s*(?:<<|>>)\s*\d+"),
        "an inline xorshift step",
        "dp_xs32 / dp_xs64",
    ),
    (
        # The same shape spelled out: `x = x ^ (x << 13)`.
        re.compile(r"(\*?\w+)\s*=\s*\1\s*\^\s*\(?\s*\1\s*(?:<<|>>)\s*\d+"),
        "an inline xorshift step",
        "dp_xs32 / dp_xs64",
    ),
    (
        re.compile(r"sqrt\s*\(\s*-\s*(2\.0\s*\*\s*)?log\s*\("),
        "a hand-rolled Box-Muller transform",
        "dp_gauss / dp_cgauss / dp_gauss64",
    ),
    (
        re.compile(r"/\s*4294967297\.0"),
        "the private (x + 1) / (2^32 + 1) uniform mapping",
        "dp_uni",
    ),
    (
        re.compile(r"/\s*9007199254740993\.0"),
        "the private 53-bit uniform mapping",
        "dp_uni64",
    ),
]

RNG_HOME = "dp_rng_test.h"


def strip_comments(text: str) -> str:
    """Blank out C comments, preserving line numbering.

    Required, not tidiness: `dp_rng_test.h` documents the broken generator it
    replaced by QUOTING it, and `test_costas_core.c` explains in prose what it
    no longer does. A scanner that reads comments would fire on the
    documentation of the very rule it enforces — the failure mode where
    describing a detector's target blinds or trips the detector.
    """
    out = []
    for chunk in re.split(r"(/\*.*?\*/|//[^\n]*)", text, flags=re.S):
        if chunk.startswith("/*") or chunk.startswith("//"):
            out.append("".join(c if c == "\n" else " " for c in chunk))
        else:
            out.append(chunk)
    return "".join(out)


def generators() -> list[str]:
    """Fail any test that rolls its own random source."""
    bad = []
    for path in sorted(TESTS.glob("*.c")) + sorted(TESTS.glob("*.h")):
        if path.name == RNG_HOME:
            continue
        code = strip_comments(path.read_text())
        for n, line in enumerate(code.splitlines(), 1):
            for pattern, what, use in GENERATOR_IDIOMS:
                if pattern.search(line):
                    bad.append(
                        f"{path.relative_to(ROOT)}:{n}: {what} — "
                        f"use {use} from {RNG_HOME}"
                    )
    return bad


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
    # An empty result set is not a pass, in either half of this gate. The
    # glibc gate went green on a missing .so and the C tarball gate on an
    # empty tarball; both looked exactly like this code path.
    for required in (HEADER, TESTS / RNG_HOME):
        if not required.exists():
            print(
                f"check_tests_ssot: {required} is missing — nothing to "
                "enforce,"
            )
            print("  so this gate has not run, so it has not passed.")
            return 1

    macros, funcs = provided()
    if not macros or not funcs:
        print("check_tests_ssot: parsed NO names out of the dp_*.h")
        print("  family — the scan found nothing, so it did not run.")
        return 1

    # The family glob must actually reach the founding members. A glob that
    # silently misses one leaves this gate reporting a healthy count while
    # covering less than it used to -- which is what `dp_*_test.h` did to
    # `dp_test.h`, and is the same smaller-denominator failure the assertion
    # ratchet below exists to catch.
    for anchor, macro in (
        ("dp_test.h", "DP_CHECK"),
        (RNG_HOME, "DP_RNG_TWO_PI"),
    ):
        if macros.get(macro) != anchor:
            print(
                f"check_tests_ssot: {macro} is not attributed to {anchor} — "
                "the family"
            )
            print("  glob is not reaching it, so this gate covers less than")
            print("  it claims. Not a pass.")
            return 1

    owners = {p.name for p in family()}
    bad: list[str] = []
    for path in sorted(TESTS.glob("*.c")) + sorted(TESTS.glob("*.h")):
        # A family header legitimately defines what it owns. It must still not
        # re-define what a SIBLING owns, so only its own names are skipped.
        mine = path.name if path.name in owners else None
        for n, line in enumerate(path.read_text().splitlines(), 1):
            m = re.match(r"\s*#\s*define\s+(\w+)", line)
            if m:
                name = m.group(1)
                if name in macros and macros[name] != mine:
                    bad.append(
                        f"{path.relative_to(ROOT)}:{n}: re-defines "
                        f"{name}, which {macros[name]} provides"
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
                if cand in RETIRED:
                    bad.append(
                        f"{path.relative_to(ROOT)}:{n}: re-implements "
                        f"{cand} — it was retired"
                    )
                elif cand in funcs and funcs[cand] != mine:
                    bad.append(
                        f"{path.relative_to(ROOT)}:{n}: re-implements "
                        f"{cand} — {funcs[cand]} exports it"
                    )

    bad += generators()

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
        f"{len(macros)} macros and {len(funcs)} helpers owned by "
        f"{len(family())} dp_*.h harness headers; no private RNG"
        + (f"; no file lost assertions vs {base}" if base else "")
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
