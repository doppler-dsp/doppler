#!/usr/bin/env python3
"""A finding is cited by its key or its claim, never by its number.

A validation report numbers its findings by position (``F1`` for the first
``find()``), so a number goes stale the moment a finding before it is
dropped or reordered. Inside a report that is now handled at render:
prose cites ``R.ref(key)`` and ``Report._self_check`` refuses a typed
number (#2059). Everything OUTSIDE a report -- a C test's comment, a
header, a design page, an example -- has no render step to resolve a
number, so it cites the finding's key (```filter_on_cn0_not_margin```)
or says the claim in words. #2056 dropped one PSD finding and left five
citations pointing at the wrong one, two of them outside the report;
conv's executive summary had cited a finding that never existed since
the day it was written.

**Registration-free.** It scans every tracked text file (``git
ls-files``), so a new test, page or example is checked the moment it is
committed.

**What it refuses.** ``F`` followed by a number from 1 to 99, as a whole
word. ``F0`` (a frequency) is not a finding number, and ``F32``/``F64``
are type names, so those pass.

**What it skips, and why.** Generated files, whose source is scanned or
resolves its own citations: a rendered ``results.md`` (its numbers come
from ``render()``), the validation log built from them, ``docs/c-api``
(built from the headers), ``*.pyi`` stubs (built from the headers),
vendored code. ``CHANGELOG.md``, because a released entry records what
the report said at that release. ``docs/dev/issues.md`` and
``issue-tiers.toml``, which carry GitHub's issue titles verbatim. And the
files whose subject IS the numbering: the report framework, its tests,
the two report gates and this gate.

Usage
-----
    python3 scripts/check_finding_citations.py           # exit 1 on a hit
    python3 scripts/check_finding_citations.py --root D  # scan another tree
"""

from __future__ import annotations

import argparse
import fnmatch
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

#: A typed finding number: F1-F99 as a word, but not the type names.
TYPED = re.compile(r"\bF(?!32\b|64\b)[1-9]\d?\b")

#: The text files scanned.
SUFFIXES = ("*.c", "*.h", "*.py", "*.md", "*.toml", "*.cmake", "*.rs")

#: Skipped, each for the reason in the module docstring.
SKIP = (
    "*/results.md",
    "docs/dev/contributing/validation-log.md",
    "docs/c-api/*",
    "*.pyi",
    "vendor/*",
    "CHANGELOG.md",
    "docs/dev/issues.md",
    "docs/dev/issue-tiers.toml",
    "src/doppler/tests/_validation_common.py",
    "src/doppler/tests/test_validation_report.py",
    "scripts/gen_validation_log.py",
    "scripts/check_validation_reports.py",
    "scripts/check_finding_citations.py",
    "src/doppler/tests/test_finding_citations_gate.py",
)


def tracked(root: Path) -> list[str]:
    """Every tracked file the gate reads, as git names it."""
    out = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z", "--", *SUFFIXES],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    return [
        p
        for p in out.split("\0")
        if p and not any(fnmatch.fnmatch(p, s) for s in SKIP)
    ]


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    # A gate that can only run against the real tree cannot be sabotaged
    # without breaking doppler, so its test points it at a seeded one.
    ap.add_argument("--root", type=Path, default=ROOT)
    root = ap.parse_args(argv[1:]).root.resolve()

    files, bad = 0, []
    for rel in tracked(root):
        try:
            text = (root / rel).read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        files += 1
        for n, line in enumerate(text.splitlines(), 1):
            for m in TYPED.finditer(line):
                bad.append((f"{rel}:{n}", m.group(0), line.strip()))

    # A scan that read nothing is broken, not clean.
    if files == 0:
        print("check_finding_citations: read no file -- the scan is broken")
        return 1
    # Every message is ASCII: a Windows console's code page (cp1252) prints
    # an em-dash as a replacement character.
    if bad:
        print("check_finding_citations: a finding is cited by number -- FAIL")
        for where, tag, line in bad:
            print(f"  {where}: {tag}  | {line[:70]}")
        print(
            "\n  Findings are numbered by position, so a number re-points"
            "\n  when one before it is dropped (#2059). Cite the finding's"
            "\n  key, as its validate.py records it (`pfa_over_delivered`),"
            "\n  or say the claim in words."
        )
        return 1
    print(f"check_finding_citations: OK -- {files} file(s), none typed")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
