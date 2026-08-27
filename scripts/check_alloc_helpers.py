#!/usr/bin/env python3
"""Gate: a trusted internal allocation goes through the abort-on-OOM helper.

`native/inc/clib_common.h` carries `dp_xmalloc`, `dp_xcalloc` and `dp_xnn` --
the classic GNU `xmalloc` pattern -- and the house rule they exist for is in
CLAUDE.md: *no error handling for impossible scenarios; trust internal
guarantees.* A small, fixed-size, argument-validated internal allocation can
only return NULL on genuine OOM, so threading a per-call
`if (!p) { cleanup; return NULL; }` writes an unwind path no test can reach.
It is uncoverable by construction, and it tanks the patch-coverage gate that
every changed C line has to satisfy.

The helpers were added 2026-07-21 and adopted on three cores, which raised
that branch's C patch coverage 85% -> 92%. Then the rule lived in a memory
file and nowhere else, so nothing extended it and nothing stopped a new bare
`malloc` landing beside a helper call. **This is the gate.** It was written
after that memory was archived as "enforced" and a check found the enforcement
did not exist: a bare `malloc` passed every gate the repo had.

What this does NOT do is ban allocation. A caller-sized, grow-on-demand buffer
CAN fail on a length the caller chose, and handling that is correct. The gate
cannot tell the two apart from a regex, so it does not try: it counts, and
compares against a checked-in baseline.

**The baseline is a RATCHET.** A file may allocate no more times than it
already does; a file not listed may not allocate at all. Convert some and the
count must come DOWN -- a baseline that kept its old number after a file
improved is a waiver outliving its reason, which is the failure the Python
hollow-benchmark ratchet shipped with and had to be fixed for.

Counts are per FILE rather than per line, deliberately. The sibling gate
`check_phase_conversion_sites.py` keys on the normalised source line, which is
right for a heterogeneous list where each entry needs its own reason -- but
two identical `x = malloc (n);` lines in one file collapse to one key, so a
third could be added without the gate noticing. Every entry here has the same
reason (it predates the helpers), so nothing is lost by counting, and a count
cannot be fooled by duplicate lines or churn when one moves.

Usage:
    python3 scripts/check_alloc_helpers.py [--root DIR] [--allow FILE]
    python3 scripts/check_alloc_helpers.py --update-baseline

Run it with `make lint-alloc-helpers`; `make lint` includes it and a
pre-commit hook dispatches to the same target.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ALLOW = Path(__file__).parent / ".alloc-helper-allow"

#: Library C. Tests, benchmarks and validation harnesses are out of scope --
#: they are oracles, they allocate freely, and a crash there is a test
#: failure rather than a shipped abort.
SCAN_DIRS = ("native/inc", "native/src")

#: The helpers' own home: `dp_xmalloc` IS `dp_xnn (malloc (n))`, so the one
#: file that must contain a bare allocation is this one.
SANCTIONED = "native/inc/clib_common.h"

#: `<mod>_ext*.c` is jm-generated CPython glue. It allocates numpy output
#: buffers whose size comes from the CALLER, checks for NULL and raises
#: MemoryError -- correct, reachable error handling -- and jm owns the code
#: either way, so a gate here would be a gate against the generator.
GENERATED = re.compile(r"_ext(_[a-z0-9_]+)?\.c$")

#: `dp_xmalloc (` must not match: the character before `malloc` is `x`, which
#: the look-behind rejects. `p->realloc_hook` likewise, via the `.`/`>` guard.
ALLOC = re.compile(r"(?<![\w.>])(malloc|calloc|realloc)\s*\(")


def in_scope(rel: str) -> bool:
    """Is this a hand-written library C file the rule applies to?"""
    if rel == SANCTIONED or GENERATED.search(rel):
        return False
    return rel.startswith(SCAN_DIRS) and rel.endswith((".c", ".h"))


def counts(root: Path) -> dict[str, int]:
    """Bare allocation calls per in-scope file, omitting files with none."""
    found: dict[str, int] = {}
    for d in SCAN_DIRS:
        base = root / d
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if not path.is_file():
                continue
            rel = path.relative_to(root).as_posix()
            if not in_scope(rel):
                continue
            n = len(ALLOC.findall(path.read_text(errors="replace")))
            if n:
                found[rel] = n
    return found


def sites(root: Path, rel: str) -> list[tuple[int, str]]:
    """Every offending line in one file, for the failure message."""
    text = (root / rel).read_text(errors="replace")
    return [
        (n, line.strip())
        for n, line in enumerate(text.splitlines(), 1)
        if ALLOC.search(line)
    ]


def load_allow(path: Path) -> dict[str, int]:
    """The ratchet: `<path> <count>` per line, `#` comments ignored."""
    allowed: dict[str, int] = {}
    if not path.exists():
        return allowed
    for raw in path.read_text().splitlines():
        line = raw.split("#")[0].strip()
        if not line:
            continue
        rel, _, n = line.rpartition(" ")
        allowed[rel.strip()] = int(n)
    return allowed


HEADER = """\
# Baseline for scripts/check_alloc_helpers.py -- a RATCHET.
#
# Bare malloc/calloc/realloc calls per file in doppler's library C, all of
# them predating the dp_xmalloc/dp_xcalloc/dp_xnn helpers in
# native/inc/clib_common.h.
#
# EVERY COUNT MAY ONLY SHRINK, and a file not listed may not allocate at
# all. Converting a call means the number here comes down -- re-record with
#
#     make lint-alloc-helpers-baseline
#
# A count that stayed high after its file improved is a waiver outliving
# its reason, so the gate fails on that too.
#
# Format:  <path> <count>
"""


def write_baseline(path: Path, found: dict[str, int]) -> None:
    body = "".join(f"{rel} {n}\n" for rel, n in sorted(found.items()))
    path.write_text(HEADER + "\n" + body)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--allow", type=Path, default=None)
    ap.add_argument(
        "--update-baseline",
        action="store_true",
        help="re-record the ratchet after converting some call sites",
    )
    a = ap.parse_args()
    root = a.root.resolve()
    allow = a.allow or (root / ALLOW.relative_to(ROOT))

    found = counts(root)

    if a.update_baseline:
        write_baseline(allow, found)
        total = sum(found.values())
        print(
            f"alloc-helper gate: baseline re-recorded -- {total} call(s) "
            f"across {len(found)} file(s)"
        )
        return 0

    allowed = load_allow(allow)
    problems: list[str] = []

    for rel, n in sorted(found.items()):
        cap = allowed.get(rel)
        if cap is None:
            problems.append(
                f"  {rel}: {n} bare allocation(s), and this file is not in "
                f"the baseline at all"
            )
            for ln, text in sites(root, rel):
                problems.append(f"      {rel}:{ln}  {text}")
        elif n > cap:
            problems.append(
                f"  {rel}: {n} bare allocation(s), baseline allows {cap}"
            )
            for ln, text in sites(root, rel):
                problems.append(f"      {rel}:{ln}  {text}")

    if problems:
        print(
            "FAIL: a bare allocation appeared in doppler's library C.\n"
            "\n"
            "A trusted internal allocation -- fixed size, arguments already\n"
            "validated -- can only fail on genuine OOM, so it goes through\n"
            "the abort-on-OOM helpers in native/inc/clib_common.h:\n"
            "\n"
            "    s->buf = dp_xmalloc (n * sizeof *s->buf);\n"
            "    s->tab = dp_xcalloc (n, sizeof *s->tab);\n"
            "    s->kid = dp_xnn (kid_create (...));\n"
            "\n"
            "That removes an unwind path no test can reach, which is the\n"
            "point: it is uncoverable by construction and the patch-coverage\n"
            "gate has to be satisfied by every changed C line.\n"
            "\n"
            "If the size comes from the CALLER and the failure is genuinely\n"
            "reachable, handling it is correct -- raise this file's count in\n"
            f"  {allow.name}\n"
            "and say why in the commit message.\n"
        )
        print("\n".join(problems))
        return 1

    stale = []
    for rel, cap in sorted(allowed.items()):
        n = found.get(rel, 0)
        if not (root / rel).exists():
            stale.append(f"  {rel}: listed, but the file is gone")
        elif n < cap:
            stale.append(
                f"  {rel}: baseline allows {cap}, file now has {n} -- "
                f"the ratchet has to come down with it"
            )

    if stale:
        print(
            "FAIL: the baseline is a ratchet and has gone slack.\n"
            "\n"
            "These files improved and their allowance did not follow, so it\n"
            "is now a waiver outliving its reason -- the next bare\n"
            "allocation added to one of them would pass. Re-record with\n"
            "  make lint-alloc-helpers-baseline\n"
        )
        print("\n".join(stale))
        return 1

    total = sum(found.values())
    print(
        f"alloc-helper gate: OK -- {total} bare allocation(s) across "
        f"{len(found)} file(s), all ratcheted (may only shrink); "
        f"sanctioned home is {SANCTIONED}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
