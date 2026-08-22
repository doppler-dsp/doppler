#!/usr/bin/env python3
"""Promote ``changelog.d/`` fragments into ``CHANGELOG.md``'s ``[Unreleased]``.

An entry is written as a file so two pull requests never touch the same line
(``changelog.d/README.md`` has the measurement that forced it). This is the one
step that folds them back in, run once per release instead of once per PR.

The directory a fragment sits in IS the ``### Heading`` it lands under, so a
section is never declared twice and cannot disagree with itself. Sections are
emitted in the Keep a Changelog order rather than alphabetically, and a heading
that does not exist yet is created in that order; one that does is appended to,
newest fragments last, so an existing hand-written entry keeps its position.

Idempotent by construction: fragments are deleted as they are promoted, so a
second run finds nothing and changes nothing.

Usage
-----
``make changelog-assemble`` — the only supported entry point (the Makefile is
the SSOT for how tools run). ``--check`` reports what would move and exits 1 if
anything would, which is what lets a release gate ask "is anything still
sitting unassembled?" without mutating the tree.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
CHANGELOG = ROOT / "CHANGELOG.md"
FRAGMENTS = ROOT / "changelog.d"

#: Keep a Changelog's order, plus the ``Breaking`` this repo puts first when it
#: has one. Order is fixed here rather than derived from the directory listing
#: because a reader scanning a release wants the breaking news first and the
#: filesystem has no opinion about that.
SECTIONS = [
    "breaking",
    "added",
    "changed",
    "deprecated",
    "removed",
    "fixed",
    "security",
]

UNRELEASED = re.compile(r"^## \[Unreleased\]\s*$", re.M)
NEXT_RELEASE = re.compile(r"^## \[", re.M)


def fragments() -> dict[str, list[pathlib.Path]]:
    """Every fragment, grouped by section and sorted within it.

    Sorted by filename so the assembled order is a property of the tree and
    not of the order the filesystem happened to return.
    """
    found: dict[str, list[pathlib.Path]] = {}
    for section in SECTIONS:
        d = FRAGMENTS / section
        if not d.is_dir():
            continue
        files = sorted(p for p in d.glob("*.md") if p.name != "README.md")
        if files:
            found[section] = files
    return found


def unreleased_bounds(text: str) -> tuple[int, int]:
    """Character offsets of the ``[Unreleased]`` body.

    Returns ``(start, end)`` where ``start`` is just past the heading line and
    ``end`` is the start of the next ``## [`` heading (or end of file).
    """
    m = UNRELEASED.search(text)
    if not m:
        sys.exit(
            "changelog-assemble: no '## [Unreleased]' heading in CHANGELOG.md"
        )
    start = m.end()
    nxt = NEXT_RELEASE.search(text, start)
    return start, (nxt.start() if nxt else len(text))


def insert(body: str, section: str, entries: str) -> str:
    """Put @p entries under ``### <section>``, creating the heading if absent.

    An existing heading is APPENDED to, so a hand-written entry already there
    keeps its place and the fragments follow it.
    """
    title = section.capitalize()
    head = re.compile(rf"^### {title}\s*$", re.M)
    m = head.search(body)
    if m:
        nxt = re.compile(r"^### ", re.M).search(body, m.end())
        cut = nxt.start() if nxt else len(body)
        return body[:cut].rstrip("\n") + "\n\n" + entries + "\n" + body[cut:]

    # Create it, in SECTIONS order: find the first existing heading that should
    # come after this one and insert above it; otherwise append.
    later = SECTIONS[SECTIONS.index(section) + 1 :]
    for nxt_section in later:
        m2 = re.compile(rf"^### {nxt_section.capitalize()}\s*$", re.M).search(
            body
        )
        if m2:
            return (
                body[: m2.start()].rstrip("\n")
                + f"\n\n### {title}\n\n"
                + entries
                + "\n\n"
                + body[m2.start() :]
            )
    return body.rstrip("\n") + f"\n\n### {title}\n\n" + entries + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--check",
        action="store_true",
        help="report what would move; exit 1 if anything would",
    )
    args = ap.parse_args()

    found = fragments()
    if not found:
        print("changelog-assemble: no fragments — nothing to promote")
        return 0

    total = sum(len(v) for v in found.values())
    if args.check:
        print(f"changelog-assemble: {total} fragment(s) still unassembled")
        for _section, files in found.items():
            for f in files:
                print(f"  {f.relative_to(ROOT)}")
        return 1

    text = CHANGELOG.read_text()
    start, end = unreleased_bounds(text)
    body = text[start:end]

    for section, files in found.items():
        chunks = []
        for f in files:
            entry = f.read_text().strip("\n")
            if not entry.lstrip().startswith("- "):
                sys.exit(
                    f"changelog-assemble: {f.relative_to(ROOT)} does "
                    "not start with '- ' — a fragment is the entry "
                    "itself, verbatim"
                )
            # A fragment may not carry a `### ` heading. The DIRECTORY is the
            # heading, so one in the body is a second, contradictory answer --
            # and because the fragments for a section are joined and inserted
            # as ONE block, a stray heading captures every fragment that sorts
            # after it. Measured on the 0.43.0 assembly:
            # `changed/rx-battery-run-level-gate.md` ended with a `### Removed`
            # line, and the five `changed/` fragments sorting after it were
            # published under **Removed** -- a static-link fix and four
            # example rewrites, all announced as deletions. The bullet count
            # was right, which is exactly why this needs a gate: a miscounted
            # entry is visible and a misfiled one is not.
            for n, line in enumerate(entry.splitlines(), 1):
                if line.startswith("### "):
                    sys.exit(
                        f"changelog-assemble: {f.relative_to(ROOT)} line {n} "
                        f"is a section heading ({line.strip()!r}).\n"
                        "  The directory a fragment sits in IS its heading, "
                        "so this is a second answer to the same question --\n"
                        "  and it captures every fragment promoted after it. "
                        "Delete the line; move the\n"
                        "  entry to changelog.d/<section>/ if it belongs "
                        "under a different one."
                    )
            chunks.append(entry)
        body = insert(body, section, "\n\n".join(chunks))

    CHANGELOG.write_text(text[:start] + body + text[end:])

    for files in found.values():
        for f in files:
            f.unlink()

    print(
        f"changelog-assemble: promoted {total} fragment(s) into [Unreleased]"
    )
    for section, files in found.items():
        print(f"  ### {section.capitalize()}: {len(files)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
