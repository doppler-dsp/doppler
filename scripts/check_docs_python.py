#!/usr/bin/env python3
"""Refuse a docs build on a Python the docs toolchain does not support.

The docs group's floor is Python 3.11: pyproject.toml's docs group names
the versions, and the older set that supported 3.10 is the one that grew
past 4 GB on a cold build (#2840). On
3.10 the group installs NOTHING, and the first sign would be a missing
``zensical`` command; this says why instead.

Usage
-----
    python scripts/check_docs_python.py [--version X.Y]
"""

from __future__ import annotations

import argparse
import sys

FLOOR = (3, 11)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--version", help="X.Y to test instead of this Python")
    a = ap.parse_args(argv)
    have = (
        tuple(int(p) for p in a.version.split("."))
        if a.version
        else sys.version_info[:2]
    )
    if have < FLOOR:
        print(
            f"docs: Python {have[0]}.{have[1]} cannot build the docs; they "
            f"need Python >= {FLOOR[0]}.{FLOOR[1]} (the docs group's "
            "floor). Use a newer interpreter: "
            "`uv run --python 3.12 ...`",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
