#!/usr/bin/env python3
"""A design page states what IS. History goes to the record beside it.

Why this gate exists
--------------------

``docs/design/`` is where a reader goes to learn what a thing *is*. Above
roughly 350 lines that stops being true here, and always in the same four
ways: a ``**Status:**`` preamble that decodes to "which parts of this page
are real"; a terminal ``## The record`` / ``## The work that answered it``;
dated sections appended as the work proceeded; and open questions parked in
``design/`` rather than filed.

``async-dsss-receiver.md`` is the extreme. It was split once, on 2026-09-07,
when it reached **3686 lines** -- the dated record moved to a companion
``-measurements.md`` page keeping the same section numbers, because issues and
harnesses cite ``§12.17``. The room bought by that split refilled: the design
page was back to 2270 lines and 17% play-by-play before this gate existed.

That is the whole argument for gating rather than tidying. The convention was
already written down (``[[feedback-design-pages-state-what-is]]``, and
``docs/dev/contributing/adding-algorithms.md``), it was applied once by hand,
and it did not hold. A rule with no gate is a wish.

What it checks
--------------

Three patterns, each a heading or a preamble -- never prose. Prose about the
past is fine and often necessary; a *section* of it is the defect, because a
section is what a reader has to navigate around.

1. **A dated section heading** -- ``## 12.28 What was measured (2026-09-10)``.
   A design page has no dates. If a fact needs a date it is a measurement, and
   measurements have a page.
2. **A status/phase preamble** -- ``**Status:** built through §8.3 step 6``,
   ``*Phase 1 of adding an algorithm*``, ``*Nothing below is implemented.*``.
   These tell a reader which parts of the page to disbelieve, which is a
   property of the work rather than of the thing.
3. **A record-family heading** -- ``The record``, ``The work that answered
   it``, ``What was measured``, ``Open questions``, ``does not settle``.

What is exempt, and why
-----------------------

``*-measurements.md`` is exempt **by name**. That is the companion page the
convention creates: it is *supposed* to be a dated record, and it is where
rule 1 and rule 3 material belongs. Naming the exemption after the convention
means a new companion page is covered the moment it is created, with nothing
to register.

``archive/`` is exempt, as it is by every other docs gate here.

The ratchet
-----------

``docs/.design-page-baseline`` lists the violations that exist today, one
``<path>\t<rule>`` per line. It fails on:

- a **new** violation absent from the baseline, and
- a **baseline entry that no longer violates**.

The second half is what makes it a ratchet rather than an exemption list: the
file can only shrink, and it cannot rot into a set of carve-outs nobody
rereads. ``rx-test.md`` (1270 lines) and ``mpsk.md`` (1978, and 79 external
``§`` citations) cannot be fixed in one change, and pretending otherwise
would mean either a gate nobody can turn on or a gate turned on by deleting
the rule. The burn-down is doppler#1302 -- 24 entries across 15 pages, one
page per PR.

This is deliberately a **static scan of source**, not a count of anything a
run observed: a line of markdown is a property of the tree, so it can be
ratcheted honestly (doppler#1028/#1098 is the counter-example -- two attempts
to ratchet a sanitizer's *report* were defeated by the environment, twice).

Usage
-----

``check_design_pages.py [page ...]`` -- with no arguments it scans
``docs/design/**/*.md`` from the repository root. Explicit arguments are how
the gate's own test drives it over seeded pages, so that it is proven to go
red rather than merely observed to be green.

``--write`` rewrites the baseline. Use it when a page is FIXED, never to
admit a new violation.

Exit 0 when the tree matches the baseline, 1 otherwise.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASELINE = ROOT / "docs" / ".design-page-baseline"

#: A heading carrying an ISO date. `## 12.28 What was measured (2026-09-10)`.
DATED = re.compile(r"^#{1,6} .*\b20\d{2}-[01]\d-[0-3]\d\b")

#: A preamble that tells the reader which parts of the page to disbelieve.
#: Anchored at line start so a mid-sentence "status" is not a finding.
STATUS = re.compile(
    r"^\s*(?:\*\*Status:\*\*"
    r"|\*Phase \d"
    r"|.*\bNothing (?:below is implemented|here is decided)\b)",
    re.IGNORECASE,
)

#: A heading in the record family: the page narrating its own construction.
RECORD = re.compile(
    r"^#{1,6} .*(?:"
    r"The record"
    r"|The work that answered"
    r"|What was measured"
    r"|Open questions"
    r"|does not settle"
    r")",
    re.IGNORECASE,
)

RULES = (
    ("dated-heading", DATED),
    ("status-preamble", STATUS),
    ("record-heading", RECORD),
)


def is_exempt(path: Path) -> bool:
    """The companion record page, and archives.

    `-measurements.md` is the home the convention creates for exactly the
    material rules 1 and 3 forbid, so exempting it by NAME means a new
    companion is covered the moment it exists -- there is no list to update.
    """
    return path.name.endswith("-measurements.md") or "archive" in path.parts


def rel_name(path: Path) -> str:
    """Repo-relative where possible, absolute otherwise.

    The gate's own test drives it over pages in a tmp dir, which is not
    under the repository root; `relative_to` raises there rather than
    falling back, and a gate that crashes on its own test is worse than one
    that never ran.
    """
    try:
        return path.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def violations(path: Path) -> set[str]:
    """Rule names this page trips. A page trips a rule at most once."""
    hit: set[str] = set()
    in_fence = False
    for line in path.read_text(encoding="utf-8").splitlines():
        # A fenced block may legitimately show a dated heading as an example
        # -- docs about docs do exactly that.
        if line.lstrip().startswith("```"):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        for name, pattern in RULES:
            if name not in hit and pattern.match(line):
                hit.add(name)
    return hit


def read_baseline() -> set[tuple[str, str]]:
    if not BASELINE.exists():
        return set()
    out: set[tuple[str, str]] = set()
    for line in BASELINE.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        path, _, rule = line.partition("\t")
        out.add((path.strip(), rule.strip()))
    return out


def scan(paths: list[Path]) -> set[tuple[str, str]]:
    found: set[tuple[str, str]] = set()
    for p in sorted(paths):
        if is_exempt(p):
            continue
        for rule in violations(p):
            found.add((rel_name(p), rule))
    return found


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("pages", nargs="*", type=Path)
    ap.add_argument("--write", action="store_true")
    ap.add_argument(
        "--no-baseline",
        action="store_true",
        help="report every violation, ignoring the ratchet (for the tests)",
    )
    args = ap.parse_args(argv)

    pages = args.pages or sorted((ROOT / "docs" / "design").rglob("*.md"))
    pages = [p for p in pages if p.is_file()]
    if not pages:
        print(
            "check_design_pages: FAIL — no design pages found; the scan "
            "matched nothing, so it did not run, so it has not passed"
        )
        return 1

    found = scan(pages)

    if args.no_baseline:
        for path, rule in sorted(found):
            print(f"  {path}\t{rule}")
        print(f"check_design_pages: {len(found)} violation(s)")
        return 1 if found else 0

    if args.write:
        body = "\n".join(f"{p}\t{r}" for p, r in sorted(found))
        BASELINE.write_text(
            "# Design pages that still narrate their own construction.\n"
            "# This list may only SHRINK. See scripts/check_design_pages.py.\n"
            f"{body}\n",
            encoding="utf-8",
        )
        print(f"check_design_pages: baseline written, {len(found)} entries")
        return 0

    base = read_baseline()
    if args.pages:
        # A scoped run answers only for the pages it was handed. Comparing a
        # three-page run against the whole baseline would report every other
        # page as "fixed", which is how a gate's own test ends up asserting
        # the opposite of what it means.
        scanned = {rel_name(p) for p in pages}
        base = {(p, r) for p, r in base if p in scanned}
    new = found - base
    fixed = base - found

    if new:
        print(
            "check_design_pages: a design page narrates its own "
            "construction — FAIL\n"
        )
        for path, rule in sorted(new):
            print(f"  {path}\t{rule}")
        print(
            "\n  A design page states what IS. A dated section, a Status\n"
            "  preamble or a record heading is the work talking about\n"
            "  itself. The record has a home: the companion\n"
            "  <page>-measurements.md, which this gate exempts by name.\n"
        )
        return 1

    if fixed:
        print("check_design_pages: the baseline is stale — FAIL\n")
        for path, rule in sorted(fixed):
            print(f"  {path}\t{rule}  — no longer violates")
        print(
            "\n  These were fixed. Remove them:\n"
            "    uv run python scripts/check_design_pages.py --write\n"
            "  The list may only shrink, so a fixed entry left behind is\n"
            "  an exemption nobody rereads.\n"
        )
        return 1

    print(
        f"check_design_pages: OK — {len(pages)} page(s), "
        f"{len(base)} ratcheted violation(s), may only shrink"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
