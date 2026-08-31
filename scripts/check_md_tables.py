#!/usr/bin/env python3
"""Gate: a markdown table's separator must match its header.

A GFM table needs the header row and the `| --- | --- |` separator under it
to declare the SAME number of columns. When they disagree the block is not a
table at all -- the renderer emits it as a paragraph of literal `|` text, so
the page ships a row of pipes where a table should be.

Nothing caught that. `mdformat` reformats a mismatched table happily (it pads
the cells and moves on), and `make docs-check` passes: the strict build has
no opinion, and the link checker finds nothing broken because nothing IS
broken -- the text simply is not a table. It was found by a person reading
the published page.

The instance (doppler#1111, `docs/design/wfmgen.md`): a regex widening one
table's separator to four columns matched a *different* three-column table's
separator too, so its header said three and its separator said four. mdformat
then escaped the orphaned dash to `\\---`, which is what the raw file showed.
Both gates stayed green.

This is mechanical, so it is a check rather than a habit:

1. **Separator matches header.** The failure above, exactly.
2. **Body rows match too.** GFM silently drops cells past the header count
   and pads short rows, so a miscounted row loses content without any
   warning -- the same class of silent damage.

Cells are split on unescaped pipes, so `\\|` inside a cell counts as text
rather than a column break. Fenced code blocks are skipped: a table drawn
inside a ``` fence is an illustration, not a table.

Usage
-----
    python scripts/check_md_tables.py            # report + exit 1 on a break
    python scripts/check_md_tables.py --root DIR
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

#: A separator cell: dashes, optionally colon-anchored, optionally escaped by
#: a formatter that met an orphaned dash.
_SEP_CELL = re.compile(r"^\\?:?-{1,}:?$")

#: Split on pipes that are not backslash-escaped.
_SPLIT = re.compile(r"(?<!\\)\|")

#: Directories whose markdown is generated or vendored -- not ours to fix.
_SKIP_PARTS = frozenset({"c-api", "node_modules", ".venv", "site", "build"})


def _cells(line: str) -> list[str]:
    """The cells of a table row, ignoring the outer pipes."""
    s = line.strip()
    if s.startswith("|"):
        s = s[1:]
    if s.endswith("|") and not s.endswith("\\|"):
        s = s[:-1]
    return [c.strip() for c in _SPLIT.split(s)]


def _is_separator(line: str) -> bool:
    cells = _cells(line)
    return bool(cells) and all(_SEP_CELL.match(c or "") for c in cells)


def _looks_like_row(line: str) -> bool:
    return line.strip().startswith("|")


def check_file(path: Path) -> list[str]:
    """Every header/separator/body column mismatch in one file."""
    problems: list[str] = []
    lines = path.read_text(encoding="utf-8").split("\n")
    fence: str | None = None
    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        # Fenced blocks: a table inside one is an illustration.
        if fence is None:
            m = re.match(r"^\s*(`{3,}|~{3,})", line)
            if m:
                fence = m.group(1)[0] * 3
                i += 1
                continue
        else:
            if re.match(r"^\s*" + re.escape(fence), line):
                fence = None
            i += 1
            continue

        # A table is a row followed by a separator.
        if (
            _looks_like_row(stripped)
            and i + 1 < len(lines)
            and _looks_like_row(lines[i + 1])
            and _is_separator(lines[i + 1])
        ):
            want = len(_cells(stripped))
            got = len(_cells(lines[i + 1]))
            if got != want:
                problems.append(
                    f"{path}:{i + 2}: separator declares {got} column(s), "
                    f"header declares {want} -- this renders as literal "
                    f"text, not a table"
                )
            # Body rows, until the table ends.
            j = i + 2
            while j < len(lines) and _looks_like_row(lines[j]):
                n = len(_cells(lines[j]))
                if n != want:
                    problems.append(
                        f"{path}:{j + 1}: row has {n} cell(s), header "
                        f"declares {want} -- GFM drops the extras silently"
                    )
                j += 1
            i = j
            continue
        i += 1
    return problems


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    args = ap.parse_args()
    root = Path(args.root).resolve()

    problems: list[str] = []
    n_files = 0
    for md in sorted(root.rglob("*.md")):
        rel = md.relative_to(root)
        if _SKIP_PARTS & set(rel.parts):
            continue
        n_files += 1
        problems.extend(check_file(md))

    if problems:
        print("Markdown tables that do not render as tables:\n")
        for p in problems:
            print(f"  {p}")
        print(
            "\nA header and its `| --- |` separator must declare the same "
            "number of columns.\nWhen they disagree the block renders as a "
            "paragraph of literal pipes, and neither\nmdformat nor the "
            "strict docs build says a word."
        )
        return 1

    print(f"Markdown tables: OK -- {n_files} file(s), every table well formed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
