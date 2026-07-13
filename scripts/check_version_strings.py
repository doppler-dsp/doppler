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

    hits: list[str] = []
    for page in pages:
        for lineno, line in enumerate(
            page.read_text(encoding="utf-8").splitlines(), start=1
        ):
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
