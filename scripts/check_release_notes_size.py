#!/usr/bin/env python3
"""Fail when the release notes would be too large for GitHub to publish.

The GitHub Release body is capped. ``release-notes.sh`` builds that body from
the version's CHANGELOG section, and ``release.yml`` creates the release in
``github-release`` -- a job that lists ``publish-python`` in its ``needs``. So
an oversized body fails **after the version is on PyPI**, and PyPI refuses to
re-upload a version, which removes the ordinary "rerun the release" recovery.
The failure lands at the one step where it costs the most.

Measured on 2026-08-22, two weeks after v0.42.0: ``[Unreleased]`` held 199,941
characters and ``changelog.d/`` another 213,270, for a projected body of
**413,211** -- 3.3x the cap, and that is before ``action-gh-release`` appends
its own generated "What's Changed" list for 358 commits. v0.42.0's real notes
were 19,296 characters, so the overflow is not a near miss.

What is measured is what would be PUBLISHED
-------------------------------------------
CHANGELOG.md is the engineering record and is allowed to be enormous -- the
entries here routinely explain a defect better than the design doc does, and
shrinking them to fit a release page would destroy the wrong artifact. So the
version section may carry a ``### Highlights`` block, and when the whole
section will not fit that block is what ``release-notes.sh`` publishes, with a
link to the full section.

The hard failure therefore measures the **publishable** body: an over-budget
section with a Highlights block that fits is fine, and an over-budget section
without one is not. The cadence warning below keeps measuring the raw
accumulation, because that is the thing that is actually growing.

Two thresholds, one measurement
-------------------------------
The same number answers "can this ship?" and "is a release overdue?", because
deferring a release is precisely what makes it grow. Gating the size therefore
gates the cadence, and there is no second mechanism to drift out of step with
the first:

* over budget  -> **FAIL**. The release is unshippable; adding more entries
  only digs deeper.
* over ``WARN_FRACTION`` -> **warn**, naming the number and the last tag, so a
  release is cut on evidence rather than when someone remembers.

Why not a calendar
------------------
"Release every N days" is a rule about the clock, and the clock is not what
breaks. A fortnight of small changes ships fine; a fortnight like this one does
not. Deriving the pressure from the artifact that actually overflows keeps the
gate honest in both directions -- it stays quiet during a slow month and it
fires early during a fast week.

The paths are arguments rather than constants so this gate can be driven over
seeded content -- a gate only ever run against a tree that happens to pass is
a gate nobody has watched fail, which is the shape it exists to catch.

Usage
-----
::

    python scripts/check_release_notes_size.py            # projected body
    python scripts/check_release_notes_size.py --body-file notes.md
    python scripts/check_release_notes_size.py \
        --changelog CHANGELOG.md --fragments changelog.d
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHANGELOG = os.path.join(ROOT, "CHANGELOG.md")
FRAGMENTS = os.path.join(ROOT, "changelog.d")

# GitHub's documented maximum for a release body. If a release ever fails at
# `github-release` with the body under this number, GitHub moved it -- read the
# error and change it HERE, so the budget keeps one home.
GITHUB_BODY_LIMIT = 125_000

# `release-notes.sh` wraps the CHANGELOG section in an Install block, and
# `action-gh-release` appends a generated "What's Changed" list whose length
# grows with the commit count. Neither is under this script's control, so the
# budget reserves room rather than pretending the section is the whole body.
PREAMBLE_RESERVE = 1_000
GENERATED_NOTES_RESERVE = 24_000

BUDGET = GITHUB_BODY_LIMIT - PREAMBLE_RESERVE - GENERATED_NOTES_RESERVE

# Warn at half the budget. Low enough that a release is cut with room to spare,
# high enough that an ordinary week stays silent.
WARN_FRACTION = 0.5


def unreleased_section(changelog: str) -> str:
    """The body of ``## [Unreleased]``, up to the next ``## `` heading."""
    out: list[str] = []
    found = False
    with open(changelog, encoding="utf-8") as fh:
        for line in fh:
            if line.startswith("## [Unreleased]"):
                found = True
                continue
            if found and line.startswith("## "):
                break
            if found:
                out.append(line)
    return "".join(out)


def highlights_block(section: str) -> str:
    """The ``### Highlights`` subsection of a version section, or ``""``.

    Runs to the next ``### `` heading, so it is the summary alone rather than
    everything after it.
    """
    out: list[str] = []
    found = False
    for line in section.splitlines(keepends=True):
        if line.startswith("### Highlights"):
            found = True
            continue
        if found and line.startswith("### "):
            break
        if found:
            out.append(line)
    return "".join(out)


def fragment_text(fragments: str) -> tuple[str, int]:
    """Every unassembled ``changelog.d/<section>/<slug>.md``, and the count.

    These are promoted verbatim by ``make changelog-assemble``, so they are
    part of the next release's notes even though they are not in CHANGELOG.md
    yet. A gate that read only CHANGELOG.md would go green right up until the
    release commit assembled them.
    """
    parts: list[str] = []
    n = 0
    if not os.path.isdir(fragments):
        return "", 0
    for section in sorted(os.listdir(fragments)):
        d = os.path.join(fragments, section)
        if not os.path.isdir(d):
            continue
        for name in sorted(os.listdir(d)):
            if not name.endswith(".md"):
                continue
            with open(os.path.join(d, name), encoding="utf-8") as fh:
                parts.append(fh.read())
            n += 1
    return "".join(parts), n


def last_tag() -> tuple[str, str]:
    """``(tag, iso-date)`` of the most recent release tag, or ``("", "")``."""
    try:
        tag = subprocess.run(
            ["git", "describe", "--tags", "--abbrev=0"],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
        date = subprocess.run(
            ["git", "log", "-1", "--format=%cs", tag],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
        return tag, date
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "", ""


def report(raw: int, publishable: int, entries: int, summarised: bool) -> int:
    tag, date = last_tag()
    since = f" since {tag} ({date})" if tag else ""
    plural = "y" if entries == 1 else "ies"
    via = " via ### Highlights" if summarised else ""

    if publishable > BUDGET:
        print(
            f"check_release_notes_size: the publishable release body is "
            f"{publishable:,} characters, over the {BUDGET:,} budget by "
            f"{publishable - BUDGET:,} — FAIL"
        )
        print(f"  {entries} entr{plural}{since}, {raw:,} characters in full.")
        print()
        print(
            f"  GitHub caps a release body at {GITHUB_BODY_LIMIT:,} "
            "characters, and `github-release` runs"
        )
        print("  AFTER `publish-python` — so this fails once the version is")
        print("  already on PyPI, which refuses a re-upload. The release")
        print("  cannot simply be re-run.")
        print()
        if summarised:
            print(
                "  The `### Highlights` block is itself too long. Shorten it;"
            )
            print("  the full entries stay in CHANGELOG.md and are linked.")
        else:
            print("  Cut a release, or summarise: keep the full entries in")
            print(
                "  CHANGELOG.md and give `## [Unreleased]` a `### Highlights`"
            )
            print(
                "  block. `release-notes.sh` publishes that instead when the"
            )
            print("  whole section will not fit.")
        return 1

    if raw > BUDGET * WARN_FRACTION:
        print(
            f"check_release_notes_size: {raw:,} characters accumulated "
            f"({100.0 * raw / BUDGET:.0f}% of the {BUDGET:,} budget) — "
            "a release is due"
        )
        print(
            f"  {entries} entr{plural}{since}. "
            f"Publishable body: {publishable:,}{via}."
        )
        return 0

    print(
        f"check_release_notes_size: publishable body {publishable:,} "
        f"characters ({100.0 * publishable / BUDGET:.0f}% of {BUDGET:,}), "
        f"{entries} entr{plural}{since} — OK"
    )
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--body-file",
        help="measure a rendered release body instead of the projected one",
    )
    ap.add_argument(
        "--print-budget",
        action="store_true",
        help="print the character budget and exit (so shell callers do not "
        "hard-code a second copy of it)",
    )
    ap.add_argument("--changelog", default=CHANGELOG)
    ap.add_argument("--fragments", default=FRAGMENTS)
    args = ap.parse_args()

    if args.print_budget:
        print(BUDGET)
        return 0

    if args.body_file:
        with open(args.body_file, encoding="utf-8") as fh:
            body = fh.read()
        # A rendered body already carries the Install preamble, so only the
        # generated-notes reserve still has to be held back.
        limit = GITHUB_BODY_LIMIT - GENERATED_NOTES_RESERVE
        size = len(body)
        if size > limit:
            print(
                f"check_release_notes_size: rendered body is {size:,} "
                f"characters, over the {limit:,} budget — FAIL"
            )
            return 1
        print(
            f"check_release_notes_size: rendered body is {size:,} characters "
            f"({100.0 * size / limit:.0f}% of {limit:,}) — OK"
        )
        return 0

    section = unreleased_section(args.changelog)
    frags, _ = fragment_text(args.fragments)
    whole = section + frags
    hl = highlights_block(section)

    # Count the real entries, not the summary's own bullets: Highlights
    # summarises the release, so counting it would make the remedy inflate the
    # number that reports the problem.
    counted = whole.replace(hl, "", 1) if hl else whole
    entries = counted.count("\n- ") + counted.startswith("- ")

    raw = len(whole)
    # Highlights only stands in when the full section will not fit; below the
    # budget the full section is better notes and is what gets published.
    summarised = bool(hl.strip()) and raw > BUDGET
    publishable = len(hl) if summarised else raw
    return report(raw, publishable, entries, summarised)


if __name__ == "__main__":
    sys.exit(main())
