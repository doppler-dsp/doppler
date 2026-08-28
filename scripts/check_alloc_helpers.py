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

**The baseline is a RATCHET, and a raise needs a REASON ON THE LINE.** A
file may allocate no more times than the baseline says; a file not listed may
not allocate at all. Convert some and the count must come DOWN -- a baseline
that kept its old number after a file improved is a waiver outliving its
reason, which is the failure the Python hollow-benchmark ratchet shipped with
and had to be fixed for.

Raising a count is allowed, because the legitimate case above is real and a
regex cannot see it -- but only with `# <reason>` on that line, and the gate
compares against the BASE REF to know a raise happened. Two rules used to be
written here and they contradicted each other: this docstring and the allow
file's header both said counts may only shrink, while the failure message
said to raise the count and say why in the commit message. A commit message
is also the wrong place: it is read once, by whoever is already convinced.
The reason belongs beside the number, where the next person to look at the
line sees it.

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
import subprocess
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


def load_allow_text(text: str) -> dict[str, tuple[int, str]]:
    """`<path> <count>[  # reason]` per line -> {path: (count, reason)}.

    The reason is kept rather than stripped: it is what makes a RAISE
    reviewable, so it has to survive parsing.
    """
    allowed: dict[str, tuple[int, str]] = {}
    for raw in text.splitlines():
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        body, _, reason = raw.partition("#")
        body = body.strip()
        if not body:
            continue
        rel, _, n = body.rpartition(" ")
        allowed[rel.strip()] = (int(n), reason.strip())
    return allowed


def load_allow(path: Path) -> dict[str, tuple[int, str]]:
    """The ratchet, from disk."""
    if not path.exists():
        return {}
    return load_allow_text(path.read_text())


def allow_at(
    root: Path, ref: str, rel_allow: str
) -> dict[str, tuple[int, str]] | None:
    """The ratchet as of `ref`, or None when the ref cannot be resolved.

    None is NOT "no raises": the caller reports it as a failure, because a
    ratchet that cannot read its baseline has not been checked. Mirrors
    check_tests_ssot.py, which learned the same lesson.

    An empty dict is the third answer -- "no baseline to compare against,
    and that is correct here" -- for a --root that is not a git repo.
    """
    # NOT a git repo at all -> the raise check does not APPLY. That is this
    # gate's own test harness, which runs it against a synthetic tree via
    # --root, and a synthetic tree has no history to have raised anything
    # in. Distinct from "a repo whose ref will not resolve", which is the
    # shallow clone and IS a failure: there the baseline exists and could
    # not be read.
    if (
        subprocess.run(
            ["git", "rev-parse", "--git-dir"],
            cwd=root,
            capture_output=True,
            text=True,
        ).returncode
        != 0
    ):
        return {}

    mb = subprocess.run(
        ["git", "merge-base", "HEAD", ref],
        cwd=root,
        capture_output=True,
        text=True,
    )
    base = (
        mb.stdout.strip() if mb.returncode == 0 and mb.stdout.strip() else ref
    )
    show = subprocess.run(
        ["git", "show", f"{base}:{rel_allow}"],
        cwd=root,
        capture_output=True,
        text=True,
    )
    if show.returncode != 0:
        return None
    return load_allow_text(show.stdout)


HEADER = """\
# Baseline for scripts/check_alloc_helpers.py -- a RATCHET.
#
# Bare malloc/calloc/realloc calls per file in doppler's library C, all of
# them predating the dp_xmalloc/dp_xcalloc/dp_xnn helpers in
# native/inc/clib_common.h.
#
# A COUNT MAY ONLY SHRINK, UNLESS THE LINE CARRIES A REASON. A file not
# listed may not allocate at all. Converting a call means the number here
# comes down -- re-record with
#
#     make lint-alloc-helpers-baseline
#
# A count that stayed high after its file improved is a waiver outliving
# its reason, so the gate fails on that too.
#
# Raising one is allowed for the case the gate cannot see -- a CALLER-sized
# allocation whose failure is genuinely reachable and already handled -- but
# the reason goes on the line, not in a commit message that is read once by
# whoever is already convinced. The gate compares against origin/main to know
# a raise happened, and refuses an unexplained one.
#
# Format:  <path> <count>[  # why this file is allowed to have gone UP]
"""


def write_baseline(path: Path, found: dict[str, int]) -> None:
    """Re-record, PRESERVING each line's reason.

    Without this, `make lint-alloc-helpers-baseline` would silently strip
    every justification the moment anyone converted a call somewhere else --
    a re-record is a routine, unrelated action, and it must not delete the
    only durable record of why a count is what it is.
    """
    kept = {rel: why for rel, (_n, why) in load_allow(path).items() if why}
    body = "".join(
        f"{rel} {n}" + (f"  # {kept[rel]}\n" if rel in kept else "\n")
        for rel, n in sorted(found.items())
    )
    path.write_text(HEADER + "\n" + body)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--allow", type=Path, default=None)
    ap.add_argument(
        "--base",
        default="origin/main",
        help="ref to compare the baseline against, to detect a raise",
    )
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
    rel_allow = allow.resolve().relative_to(root).as_posix()
    base_allowed = allow_at(root, a.base, rel_allow)
    problems: list[str] = []

    # A RAISE without a reason on the line. Checked before the counts, so the
    # message is about the policy rather than about a stray malloc.
    if base_allowed is None:
        print(
            f"FAIL: cannot read {rel_allow} at {a.base}, so a raised count\n"
            "  cannot be told from an untouched one. A ratchet that cannot\n"
            "  read its baseline has not passed.\n"
            "  Fetch it:  git fetch --no-tags --depth=1 origin \\\n"
            "               +refs/heads/main:refs/remotes/origin/main"
        )
        return 1
    raised = [
        (rel, base_allowed[rel][0], cap)
        for rel, (cap, why) in sorted(allowed.items())
        if rel in base_allowed and cap > base_allowed[rel][0] and not why
    ]
    if raised:
        print(
            "FAIL: a baseline count went UP with no reason on the line.\n"
            "\n"
            "That is allowed -- a CALLER-sized allocation whose failure\n"
            "is genuinely reachable and already handled is correct, and\n"
            "no regex can tell it from the kind that should be a helper.\n"
            "What is not allowed is raising it SILENTLY: then the ratchet\n"
            "is just a number that follows the code wherever it goes.\n"
            "\n"
            "Put the reason where the next reader will be:\n"
            "\n"
            "    native/src/app/wfmgen.c 6  # caller-sized: --bits-file\n"
        )
        for rel, was, now in raised:
            print(f"  {rel}: {was} -> {now}, and the line says nothing")
        return 1

    for rel, n in sorted(found.items()):
        entry = allowed.get(rel)
        cap = None if entry is None else entry[0]
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
            "AND put the reason on that line, which is what the gate checks:\n"
            "\n"
            "    native/src/app/wfmgen.c 6  # caller-sized: --bits-file\n"
        )
        print("\n".join(problems))
        return 1

    stale = []
    for rel, (cap, _why) in sorted(allowed.items()):
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
        f"{len(found)} file(s), all ratcheted "
        f"(shrink freely; a raise needs a reason on its line); "
        f"sanctioned home is {SANCTIONED}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
