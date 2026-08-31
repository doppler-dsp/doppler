#!/usr/bin/env python3
"""Fail when a flag `wfmgen` accepts appears in none of its guide pages.

A pre-release review of wfmgen against the lifecycle spine
(``docs/dev/contributing/adding-algorithms.md``) counted the flags the
dispatcher accepts against the flags the guide mentions, and found **50 of
56**. The missing six were not scattered: they were exactly the channel
coding stage, which had no page at all. doppler#1044 wrote that page and the
count went whole.

Nothing kept it whole. The count was a fact about one afternoon, established
by a survey nobody was going to repeat, and the next flag to land could
arrive undocumented in silence -- which is how the six got there in the first
place. This is the ratchet the review asked for and did not build.

**Every accepted flag, including every alias.** An alias exists to be typed,
so an undocumented one is a secret: `--randomize` is how `--randomise` is
spelled in the half of the world that spells it that way, the parser accepts
it, and on this gate's first run the guide had never once mentioned it. There
is deliberately no allow-list. A flag costs one table cell to document, which
is cheaper than an exemption list that goes stale and has to be audited to
find out whether its entries are still true.

The pages are DISCOVERED, not registered: a new ``docs/guide/wfmgen/*.md``
counts the moment it exists, and a page that is deleted stops counting. So
this gate cannot be satisfied by editing a list in this file.

Both directions, since doppler#1054. The reverse -- a page citing a flag
the parser no longer accepts -- is live rot: a reader copies the line and
the tool rejects it.

What made the reverse hard was ownership. Scanning pages for ``--``-shaped
tokens reports ``--build`` and ``--target``, which are correct prose about
`cmake`, and neither remedy is good: an exemption list goes stale and has to
be audited, and inferring ownership from prose context is a parser for
English that will be wrong both ways.

So the reverse question is asked only of tokens inside a **wfmgen
invocation** in a fenced block. Ownership then comes for free -- the token is
an argument to `wfmgen` or it is not -- and the two `cmake` flags stop being
a question at all. Measured before building, as the issue asked: 49 of 63
flags (78%) appear in such a fence, so the check sees most of the surface
rather than a corner of it, and there are 0 unknown tokens today.

Usage
-----
    python scripts/check_wfmgen_flag_docs.py     # report + exit 1 on a gap
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from gen_wfmgen_flag_matrix import dispatcher_flags

ROOT = Path(__file__).resolve().parent.parent
GUIDE = "docs/guide/wfmgen"
SRC = "native/src/app/wfmgen.c"
#: Flags the docs document but never RUN. May only shrink, and a stale
#: entry fails too -- see the file's own header (doppler#1143).
RATCHET = "scripts/.wfmgen-flag-exercise-ratchet"


def _mentioned(flag: str, blob: str) -> bool:
    """Is `flag` named in `blob` as a whole flag, not as a prefix?

    The look-ahead is what stops ``--interleave`` from being counted as
    documented by a page that only ever mentions ``--interleave-unit``, and
    stops ``--randomise`` from being satisfied by ``--randomised``. Both are
    live pairs in this tool, so a substring test would report the guide as
    complete while a flag sat unmentioned.
    """
    return re.search(re.escape(flag) + r"(?![A-Za-z0-9-])", blob) is not None


# A fenced block, with its info string. Shell fences are the only ones a
# `wfmgen` command line lives in.
_FENCE = re.compile(r"^```(\w*)[^\n]*\n(.*?)^```", re.S | re.M)
_SHELLISH = {"sh", "bash", "console", "shell", ""}
# A long flag, not the tail of a word or an em-dash run.
_TOKEN = re.compile(r"(?<![\w-])(--[A-Za-z][A-Za-z0-9-]*)")
# The command word, after an optional `$ ` prompt and any leading path.
_INVOKE = re.compile(r"^\$?\s*(?:\S*/)?wfmgen(?:\s|$)")


def wfmgen_command_lines(text: str):
    """Yield each logical `wfmgen ...` command line in a shell fence.

    Backslash continuations are joined first, so a flag on the second line
    of a wrapped invocation belongs to it; trailing `# comments` are dropped
    so prose inside a fence cannot be read as an argument.
    """
    for fence in _FENCE.finditer(text):
        if fence.group(1) not in _SHELLISH:
            continue
        body = fence.group(2)
        if "wfmgen" not in body:
            continue
        joined, buf = [], ""
        for raw in body.split("\n"):
            line = raw.split("#", 1)[0].rstrip()
            if line.endswith("\\"):
                buf += line[:-1] + " "
                continue
            joined.append((buf + line).strip())
            buf = ""
        if buf:
            joined.append(buf.strip())
        for cmd in joined:
            # One line can hold several commands. Attributing the whole line
            # to wfmgen would read `wfmgen ... && python plot.py --dpi 100`
            # as wfmgen taking a --dpi, which is the false positive that
            # made a naive prose scan unusable in the first place. Split on
            # the shell's own separators and keep only the segments wfmgen
            # actually runs.
            for seg in re.split(r"&&|\|\||;|\|", cmd):
                seg = seg.strip()
                if _INVOKE.match(seg):
                    yield seg


def exercise_violations(
    unexercised: set[str], allowed: set[str]
) -> tuple[list[str], list[str]]:
    """(newly unwaived, stale waivers) for the exercise ratchet.

    Pure on purpose: the gate's own meta-tests drive synthetic trees through
    the CLI, and this rule is about THIS repository's docs rather than about
    any tree's shape, so it is scoped to the real root below and tested here
    directly. Both directions matter -- a waiver that outlives its defect
    silently covers the next one.
    """
    return sorted(unexercised - allowed), sorted(allowed - unexercised)


def reverse_check(root: Path, flags: set[str]) -> int:
    """Every flag a `wfmgen` fence USES must be one the parser accepts."""
    pages = sorted((root / "docs").rglob("*.md"))
    used: dict[str, set[str]] = {}
    seen: set[str] = set()
    n_cmds = 0
    for page in pages:
        text = page.read_text(encoding="utf-8")
        for cmd in wfmgen_command_lines(text):
            n_cmds += 1
            for tok in _TOKEN.findall(cmd):
                if tok in flags:
                    seen.add(tok)
                else:
                    used.setdefault(tok, set()).add(
                        str(page.relative_to(root))
                    )

    # Fail closed, for the reason the forward direction does: zero commands
    # would make every citation vacuously valid and print OK over nothing.
    if n_cmds == 0:
        print(
            "wfmgen flag docs: FAIL -- no `wfmgen` command lines found in "
            "any docs fence.\n"
            "  Nothing to check means nothing was checked.",
            file=sys.stderr,
        )
        return 1

    if used:
        print(
            f"wfmgen flag docs: FAIL -- {len(used)} flag(s) are used in a "
            "`wfmgen` command line\n  in the docs but are NOT accepted by "
            f"{SRC}:\n",
            file=sys.stderr,
        )
        for tok, where in sorted(used.items()):
            print(f"    {tok:<24} {', '.join(sorted(where))}", file=sys.stderr)
        print(
            "\n  A reader copies the line and the tool rejects it. Either "
            "the flag was\n  renamed and the page did not follow, or the "
            "page invented it.",
            file=sys.stderr,
        )
        return 1

    # The exercise RATCHET. The count above has been printed since this gate
    # was written and read by nobody; doppler#1143 turns it into a number that
    # may not get worse. Checked both ways, because a waiver that outlives its
    # defect silently covers the next one.
    # An ABSENT ratchet is the empty waiver list, not a special case. It
    # needs no guard of its own: with nothing waived, every unexercised flag
    # reports as new, so deleting the file fails the gate LOUDLY and names
    # all fourteen. An explicit "missing file" branch was redundant against
    # the real tree and actively wrong against a synthetic one -- this gate's
    # own meta-test (`test_wfmgen_flag_docs_gate.py`) builds a temp root with
    # a handful of flags and no scripts/ directory, and the guard failed it.
    # SCOPED to the real repository. The rule says "doppler's docs may not
    # exercise fewer flags than they do today"; a synthetic tree seeded by
    # this gate's own meta-tests is not doppler's docs, and running it there
    # reported every fixture's unused flag as a regression. The comparison
    # itself is `exercise_violations`, unit-tested directly, so scoping it
    # here costs no coverage of the logic.
    if root != ROOT:
        print(
            f"wfmgen flag docs: OK — {n_cmds} `wfmgen` command line(s) across "
            f"{len(pages)} page(s) cite only flags that exist "
            f"({len(seen)} of {len(flags)} flags exercised; exercise ratchet "
            "skipped for a non-repository root)"
        )
        return 0

    ratchet_path = root / RATCHET
    allowed = (
        {
            ln.strip()
            for ln in ratchet_path.read_text(encoding="utf-8").splitlines()
            if ln.strip() and not ln.lstrip().startswith("#")
        }
        if ratchet_path.is_file()
        else set()
    )
    unexercised = flags - seen
    new, stale = exercise_violations(unexercised, allowed)

    if new or stale:
        print(
            "wfmgen flag docs: FAIL -- the exercise ratchet moved.\n",
            file=sys.stderr,
        )
        if new:
            print(
                f"  {len(new)} flag(s) are documented but run by NO `wfmgen`\n"
                f"  command line in docs/, and are not in {RATCHET}:\n",
                file=sys.stderr,
            )
            for f in new:
                print(f"    {f}", file=sys.stderr)
            print(
                "\n  Add a runnable line to a guide page that uses it. A\n"
                "  capability nobody can copy is one the docs describe and\n"
                "  nothing demonstrates.",
                file=sys.stderr,
            )
        if stale:
            print(
                f"\n  {len(stale)} entr(ies) in {RATCHET} are now EXERCISED\n"
                "  and must be deleted -- the list may only shrink:\n",
                file=sys.stderr,
            )
            for f in stale:
                print(f"    {f}", file=sys.stderr)
        return 1

    print(
        f"wfmgen flag docs: OK — {n_cmds} `wfmgen` command line(s) across "
        f"{len(pages)} page(s) cite only flags that exist "
        f"({len(seen)} of {len(flags)} flags exercised; "
        f"{len(allowed)} ratcheted, may only shrink)"
    )
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="wfmgen flag doc coverage")
    ap.add_argument("--root", type=Path, default=ROOT)
    a = ap.parse_args()
    root = a.root.resolve()

    pages = sorted((root / GUIDE).glob("*.md"))
    # Fail closed. An empty or moved guide directory would otherwise make
    # every flag vacuously documented and the gate would print OK while
    # checking nothing -- absent output is not a pass.
    if not pages:
        print(
            f"wfmgen flag docs: FAIL -- no pages under {GUIDE}/.\n"
            "  Nothing to check means nothing was checked. If the guide\n"
            "  moved, this gate has to move with it.",
            file=sys.stderr,
        )
        return 1

    flags = dispatcher_flags(root / SRC)
    blob = "\n".join(p.read_text(encoding="utf-8") for p in pages)
    missing = sorted(f for f in flags if not _mentioned(f, blob))

    if not missing:
        print(
            f"wfmgen flag docs: OK — {len(flags)} flag(s) documented "
            f"across {len(pages)} guide page(s)"
        )
        # Forward passed; now the reverse. Both or neither -- a gate that
        # answers half its question is a gate whose green means half as much.
        return reverse_check(root, set(flags))

    print(
        f"wfmgen flag docs: FAIL -- {len(missing)} of {len(flags)} flag(s) "
        f"are accepted by {SRC}\n"
        f"  but named in no page under {GUIDE}/:\n",
        file=sys.stderr,
    )
    for f in missing:
        print(f"    {f}", file=sys.stderr)
    print(
        "\n  A flag a reader cannot find is a flag they do not have. Put it\n"
        "  on the page that owns its stage -- an alias belongs beside the\n"
        "  name it aliases, not on a page of its own.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
