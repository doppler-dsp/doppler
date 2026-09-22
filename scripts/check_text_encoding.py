#!/usr/bin/env python3
"""Gate: Python in ``src/doppler`` reads text as UTF-8, explicitly.

``Path.read_text()`` with no ``encoding`` decodes with the platform's locale
codec. That is UTF-8 on Linux and macOS and **cp1252** on Windows, so a file
this repo wrote as UTF-8 -- every one of them -- raises ``UnicodeDecodeError:
'charmap' codec can't decode byte 0x81`` there the moment it holds one
non-ASCII character. The first Windows run of the Python suite
(doppler#1457) lost three whole test modules to it at collection, and 68
such reads sat in the package, library code included: a Dopplerfile or a
compose YAML with a Greek letter in it would not load on Windows.

Every read was made explicit when this gate landed, so there is no
allowlist; the first bare one fails. The lint is ruff's PLW1514 in spirit,
which is preview-only in the pinned ruff and cannot be selected alone.

Usage:  python3 scripts/check_text_encoding.py [--root DIR]
Exit 0 when no bare read_text() exists.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

#: ``.read_text()`` with nothing between the parentheses: the one spelling
#: that means "the locale codec". Any argument names an encoding (or
#: ``errors=``), which is a decision somebody made.
BARE = re.compile(r"\.read_text\(\s*\)")


def bare_reads(root: Path) -> list[tuple[str, int, str]]:
    """Every bare ``read_text()`` under ``root/src/doppler``."""
    found: list[tuple[str, int, str]] = []
    for path in sorted((root / "src" / "doppler").rglob("*.py")):
        text = path.read_text(encoding="utf-8")
        for i, line in enumerate(text.splitlines()):
            if line.lstrip().startswith("#"):
                continue  # prose about the call is not the call
            if BARE.search(line):
                rel = path.relative_to(root).as_posix()
                found.append((rel, i + 1, line.strip()))
    return found


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="checkout to scan (default: this one)",
    )
    root = ap.parse_args(argv).root
    found = bare_reads(root)
    if not found:
        print("text-encoding: every read_text() in src/doppler names utf-8")
        return 0
    print("text-encoding: read_text() without an encoding --")
    for rel, n, line in found:
        print(f"  {rel}:{n}: {line}")
    print(
        '  Write read_text(encoding="utf-8"). Without it Windows decodes\n'
        "  with cp1252 and fails on the first non-ASCII byte (doppler#1457)."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
