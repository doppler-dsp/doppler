#!/usr/bin/env python3
"""Gate: a page that needs a NATS broker links how to get one.

doppler is a NATS *client*; nothing installs or starts a server. A reader who
follows the streaming quick start, ``wfmgen --output nats://…``, the specan
socket source or a streaming example hits ``dp_sub_create failed on
nats://127.0.0.1:4222/…`` with no page saying what to run (doppler#2857).
``docs/install/broker.md`` is that page, so any page that names ``nats-server``
or a ``nats://`` endpoint must link it.

Discovered, not registered: every page under ``docs/`` is scanned. A page that
is allowed to mention a broker without linking it (the design and API
reference pages, a page that bundles its own broker) is listed in
``docs/.broker-link-exempt`` as ``<glob> <why>``; the reason is mandatory.

Usage
-----
    python scripts/check_broker_links.py
"""

from __future__ import annotations

import fnmatch
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXEMPT = "docs/.broker-link-exempt"
NEEDS = re.compile(r"nats-server|nats://")
LINKS = re.compile(r"\]\((?:[./]*install/)?broker\.md(?:#[^)]*)?\)")


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


def problems(root: Path = ROOT) -> list[str]:
    skip = exempt(root)
    out: list[str] = []
    for page in sorted((root / "docs").rglob("*.md")):
        rel = page.relative_to(root).as_posix()
        if "c-api" in page.parts or any(fnmatch.fnmatch(rel, g) for g in skip):
            continue
        text = page.read_text(encoding="utf-8")
        if NEEDS.search(text) and not LINKS.search(text):
            line = next(
                n
                for n, ln in enumerate(text.splitlines(), start=1)
                if NEEDS.search(ln)
            )
            out.append(
                f"{rel}:{line}: names a NATS broker but does not link "
                "docs/install/broker.md (how to run one); add the link, or "
                f"list the page in {EXEMPT} with a reason"
            )
    return out


def main() -> int:
    found = problems()
    if found:
        print("check_broker_links: FAILED", file=sys.stderr)
        for line in found:
            print(f"  {line}", file=sys.stderr)
        return 1
    print("check_broker_links: OK -- every page that needs a broker links one")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
