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

Discovered, not registered: every page under ``docs/`` is scanned (page
discovery and the exempt file's format are shared with the other docs gates,
in ``_docs_pages``). The clone must come BEFORE the first such command: a
reader follows a page top to bottom. Pages that
are about the tree itself (the contributor and design pages) are listed in
``docs/.clone-note-exempt`` as ``<glob> <why>``; the reason is mandatory.

Usage
-----
    python scripts/check_clone_notes.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

from _docs_pages import exempt_globs, is_exempt, pages

ROOT = Path(__file__).resolve().parent.parent
EXEMPT = "docs/.clone-note-exempt"

# A fence line that runs a command only a clone has: a make target, a built
# example, compose, one of the repo's own scripts, a CMake configure/build of
# the tree, the install walks, or `uv run` (the repo's venv).
REPO_CMD = re.compile(
    r"^\s*(?:\$ )?(?:"
    r"make [a-z]|\./build/|docker compose|"
    r"python3? (?:-m )?scripts/|bash tests/|\./scripts/|"
    r"cmake -[BS]|cmake --build|uv run"
    r")"
)
FENCE = re.compile(r"^\s*(```|~~~)")
PINNED_CLONE = re.compile(r"git\s+clone\b[^\n]*(?:--branch|\s-b\s)")


def first_repo_command(text: str) -> int | None:
    """Line number of the first repository command inside a code fence."""
    fence = None
    for n, line in enumerate(text.splitlines(), start=1):
        m = FENCE.match(line)
        if m:
            fence = None if fence == m[1] else (fence or m[1])
        elif fence and REPO_CMD.match(line):
            return n
    return None


def first_pinned_clone(text: str) -> int | None:
    for n, line in enumerate(text.splitlines(), start=1):
        if PINNED_CLONE.search(line):
            return n
    return None


def problems(root: Path = ROOT) -> list[str]:
    skip = exempt_globs(root, EXEMPT)
    out: list[str] = []
    for page in pages(root):
        rel = page.relative_to(root).as_posix()
        if is_exempt(rel, skip):
            continue
        text = page.read_text(encoding="utf-8")
        line = first_repo_command(text)
        if line is None:
            continue
        clone = first_pinned_clone(text)
        if clone is None or clone > line:
            where = "after it" if clone else "nowhere on the page"
            out.append(
                f"{rel}:{line}: runs a command that exists only in a clone, "
                f"but the pinned `git clone --branch v<version>` is {where}; "
                "put one (in a doc-version region) before the first such "
                f"command, or list the page in {EXEMPT} with a reason"
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
