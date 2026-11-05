#!/usr/bin/env python3
"""Gate: a page that tells a reader to run a REPOSITORY command says how to
get the repository.

``make build``, ``./build/native/examples/...`` and ``docker compose`` only
exist in a clone; the wheel and the release tarball carry none of them. A
reader who installed a package and copies ``make build`` gets ``No rule to
make target`` and no page saying why (doppler#2857, fix 5). So any page whose
code fence runs one must also carry a ``git clone --branch v<version>`` -- the
pinned form ``scripts/check_repo_links.py`` already requires, inside a
``doc-version`` region so each release re-stamps it.

Discovered, not registered: every page under ``docs/`` is scanned. Pages that
are about the tree itself (the contributor and design pages) are listed in
``docs/.clone-note-exempt`` as ``<glob> <why>``; the reason is mandatory.

Usage
-----
    python scripts/check_clone_notes.py
"""

from __future__ import annotations

import fnmatch
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXEMPT = "docs/.clone-note-exempt"

# A fence line that runs a command only a clone has.
REPO_CMD = re.compile(r"^\s*(?:\$ )?(?:make [a-z]|\./build/|docker compose)")
PINNED_CLONE = re.compile(r"git\s+clone\b[^\n]*(?:--branch|\s-b\s)")


def exempt(root: Path) -> list[str]:
    f = root / EXEMPT
    if not f.exists():
        return []
    out: list[str] = []
    for line in f.read_text(encoding="utf-8").splitlines():
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        glob, _, why = s.partition(" ")
        if not why.strip():
            raise SystemExit(f"{EXEMPT}: '{glob}' needs a reason")
        out.append(glob)
    return out


def first_repo_command(text: str) -> int | None:
    """Line number of the first repository command inside a code fence."""
    infence = False
    for n, line in enumerate(text.splitlines(), start=1):
        if line.lstrip().startswith("```"):
            infence = not infence
        elif infence and REPO_CMD.match(line):
            return n
    return None


def problems(root: Path = ROOT) -> list[str]:
    skip = exempt(root)
    out: list[str] = []
    for page in sorted((root / "docs").rglob("*.md")):
        rel = page.relative_to(root).as_posix()
        if "c-api" in page.parts or any(fnmatch.fnmatch(rel, g) for g in skip):
            continue
        text = page.read_text(encoding="utf-8")
        line = first_repo_command(text)
        if line is not None and not PINNED_CLONE.search(text):
            out.append(
                f"{rel}:{line}: runs a command that exists only in a clone "
                "but the page has no `git clone --branch v<version>` (in a "
                f"doc-version region); add one, or list the page in {EXEMPT} "
                "with a reason"
            )
    return out


def main() -> int:
    found = problems()
    if found:
        print("check_clone_notes: FAILED", file=sys.stderr)
        for line in found:
            print(f"  {line}", file=sys.stderr)
        return 1
    print("check_clone_notes: OK -- every page that needs a clone says how")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
