#!/usr/bin/env python3
"""Every header Doxygen reads opens a ``@file`` block, so it is checked.

``make doxygen-check`` runs a coverage pass with ``EXTRACT_ALL=NO``: any
undocumented struct field, function, macro, ``@param`` or ``@return`` in a
header is a warning, and a warning fails the gate. But with
``EXTRACT_ALL=NO`` Doxygen also skips every member of a FILE that is itself
undocumented, and a file is documented only by a ``@file`` (or ``\\file``)
block. A header without one is not reported as a gap; it simply vanishes
from the pass. On 2026-10-27, 19 headers had no such block, and their 118
undocumented members (all of ``clib_common.h``'s error codes among them,
which is why ``\\ref DP_ERR_INVALID`` failed to resolve) were invisible to
a gate that read "0 warnings".

Doxygen cannot report the missing block itself, so this script does. Its
scope is the coverage pass's own, read rather than restated:

* the ``INPUT`` directories and ``EXCLUDE_PATTERNS`` of the ``Doxyfile``
  (``+=`` continuations included), and
* the extra ``--exclude`` patterns the Makefile passes, which are the same
  ``DOXYGEN_COVERAGE_EXCLUDE`` variable the coverage pass appends to
  ``EXCLUDE_PATTERNS``. One declaration feeds both, so the two can never
  disagree about which headers are in scope.

Patterns match the way Doxygen matches them: ``fnmatch`` against the file's
path (``*/name.h`` matches at any depth).

Usage
-----
    python3 scripts/check_header_file_blocks.py [--exclude PATTERN ...]

Exit 1, naming each header, when one in scope has no ``@file`` block.
Standard library only.
"""

from __future__ import annotations

import argparse
import fnmatch
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

#: A ``@file`` or ``\\file`` command anywhere in the header. It is only
#: meaningful inside a doc comment, but a header carrying the word in plain
#: code does not exist here, and matching more loosely costs nothing.
FILE_CMD = re.compile(r"[@\\]file\b")


def doxyfile_lists(text: str) -> dict[str, list[str]]:
    """Return each tag's value list from Doxyfile text.

    Handles backslash continuations and ``TAG += value`` appends, the two
    forms the Doxyfile uses for list tags.

    >>> doxyfile_lists(
    ...     "INPUT = a b\\nEXCLUDE_PATTERNS = */x \\\\\\n  */y\\n"
    ...     "EXCLUDE_PATTERNS += */z\\n"
    ... )
    {'INPUT': ['a', 'b'], 'EXCLUDE_PATTERNS': ['*/x', '*/y', '*/z']}
    """
    tags: dict[str, list[str]] = {}
    logical = re.sub(r"\\\n", " ", text)
    for line in logical.splitlines():
        line = line.split("#", 1)[0].strip()
        m = re.match(r"([A-Z_]+)\s*(\+?=)\s*(.*)$", line)
        if not m:
            continue
        tag, op, value = m.groups()
        words = value.split()
        if op == "=":
            tags[tag] = words
        else:
            tags.setdefault(tag, []).extend(words)
    return tags


def headers_in_scope(
    root: Path, inputs: list[str], excludes: list[str]
) -> list[Path]:
    """Every ``*.h`` under the inputs that no exclude pattern matches."""
    found = []
    for entry in inputs:
        base = root / entry
        candidates = [base] if base.is_file() else sorted(base.rglob("*.h"))
        for path in candidates:
            text = str(path)
            if not any(fnmatch.fnmatch(text, p) for p in excludes):
                found.append(path)
    return found


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--exclude",
        action="append",
        default=[],
        help="an extra Doxygen exclude pattern (repeatable)",
    )
    args = ap.parse_args(argv)

    tags = doxyfile_lists((ROOT / "Doxyfile").read_text(encoding="utf-8"))
    excludes = tags.get("EXCLUDE_PATTERNS", []) + args.exclude
    headers = headers_in_scope(ROOT, tags.get("INPUT", []), excludes)

    missing = [
        h
        for h in headers
        if not FILE_CMD.search(h.read_text(encoding="utf-8"))
    ]
    if missing:
        for h in missing:
            print(f"{h.relative_to(ROOT)}: no @file block")
        print(
            f"check_header_file_blocks: {len(missing)} of {len(headers)} "
            "header(s) have no @file block, so doxygen-check's coverage "
            "pass skips every member they declare. Open each with "
            "/** @file <name> ... */."
        )
        return 1
    print(
        f"check_header_file_blocks: OK -- all {len(headers)} header(s) "
        "open a @file block"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
