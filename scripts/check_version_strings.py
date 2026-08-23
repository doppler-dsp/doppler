#!/usr/bin/env python3
"""Fail when the current release version is hand-typed into the docs.

A doc that says "current version is X" is stale the moment the next
release ships -- exactly this rotted once already (`dev/wfmgen/api.md`
hard-coded 0.33.3 and was a release behind within days). Version numbers
belong in the places releases actually bump (`pyproject.toml`,
`CMakeLists.txt`, `Cargo.toml`); prose should say "the current release"
and let the reader's installer resolve it.

This gate greps every hand-owned markdown page (README.md + docs/,
excluding the generated `c-api/`+`benchmarks.md` and the frozen
`archive/`) for the literal version currently in `pyproject.toml`. It
fires at introduction time: the PR that hand-types today's version fails
CI today, instead of the page silently going stale at the next bump.
*Old* version strings (an illustrative `--version 0.33.1` pin, a
historical decision record) don't match the current version and pass --
they are history, not claims about the present.

Usage
-----
    python scripts/check_version_strings.py   # exit 1 on any hit
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"

EXCLUDED_PARTS = {"c-api", "archive"}
EXCLUDED_RELPATHS = {"benchmarks.md"}

# Delimiters of a generated version region -- see scripts/gen_doc_versions.py.
DOC_VERSION_START = "<!-- doc-version:start -->"
DOC_VERSION_END = "<!-- doc-version:end -->"


def current_version() -> str:
    text = (ROOT / "pyproject.toml").read_text(encoding="utf-8")
    m = re.search(r'^version\s*=\s*"([^"]+)"', text, re.MULTILINE)
    if not m:
        raise SystemExit(
            "check_version_strings: no version in pyproject.toml?"
        )
    return m.group(1)


def main() -> int:
    version = current_version()
    # \b alone won't do: 0.33.4 must not match inside 10.33.40 or
    # 0.33.40. Guard both ends against adjacent digits and dots.
    needle = re.compile(rf"(?<![0-9.]){re.escape(version)}(?![0-9.])")

    pages = [ROOT / "README.md"]
    for page in sorted(DOCS.rglob("*.md")):
        rel = page.relative_to(DOCS)
        if EXCLUDED_PARTS.intersection(rel.parts):
            continue
        if str(rel) in EXCLUDED_RELPATHS:
            continue
        pages.append(page)

    # examples/*/README.md too. These were the blind spot: an example's page is
    # the most likely place to hand-type a version, because it is where install
    # instructions live -- and `examples/downstream-jm/README.md` promptly did,
    # writing today's release into a `curl` line. A page nobody scans is where
    # the rot this gate exists to stop goes to hide. Their own build trees are
    # skipped; nothing generated lives there.
    for root in ("examples", "example-projects"):
        for page in sorted((ROOT / root).rglob("*.md")):
            if "build" in page.relative_to(ROOT).parts:
                continue
            pages.append(page)

    hits: list[str] = []
    for page in pages:
        # Lines inside a `doc-version` region are GENERATED from
        # pyproject.toml by scripts/gen_doc_versions.py and gated by its
        # own --check, so they are the one place the current version is
        # allowed to appear literally. Without this the two gates
        # contradict each other: one requires the version, the other
        # forbids it, and there is no tree that satisfies both.
        generated = False
        for lineno, line in enumerate(
            page.read_text(encoding="utf-8").splitlines(), start=1
        ):
            if DOC_VERSION_START in line:
                generated = True
                continue
            if DOC_VERSION_END in line:
                generated = False
                continue
            if generated:
                continue
            if needle.search(line):
                rel = page.relative_to(ROOT)
                hits.append(f"  {rel}:{lineno}: {line.strip()}")

    if hits:
        print(
            f"check_version_strings: the current version ({version}) is "
            "hand-typed into the docs -- it will be stale at the next "
            "release. Rephrase ('the current release', 'as of this "
            "writing') or move the fact somewhere generated:",
            file=sys.stderr,
        )
        print("\n".join(hits), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
