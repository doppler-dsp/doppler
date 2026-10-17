"""Render a measurement record's tables from committed data, and check them.

A record under ``docs/design/`` that quotes a run keeps the run's data
beside it and a script that renders every one of its tables between
markers::

    <!-- <prefix>:<block>:start -->

    | ... |

    <!-- <prefix>:<block>:end -->

``--write`` rewrites the blocks; ``--check`` (a line in the Makefile's
``DOCS_CHECK_PRE_CMDS``, so in ``make docs-invariants`` and
``make docs-check``) fails when one differs from what the data renders,
cell by cell, so mdformat's column padding is not a difference. A cell is
the data's output, never an edit.

Two records render this way: the Spectrogram's
(``spectrogram-measurements/``, its runs through ``_record.py``) and the
parallel receivers' (``async-dsss-parallel-measurements/record.py``). This
module is what they share, so they cannot disagree on what a block is or
how one is checked.
"""

from __future__ import annotations

import argparse
import re
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from collections.abc import Callable, Sequence
    from pathlib import Path


def table(head: Sequence[str], rows: Sequence[Sequence[str]]) -> str:
    """A markdown table: the header, the rule, the rows."""
    out = ["| " + " | ".join(head) + " |"]
    out.append("| " + " | ".join("---" for _ in head) + " |")
    out += ["| " + " | ".join(r) + " |" for r in rows]
    return "\n".join(out)


def span(prefix: str, name: str) -> re.Pattern[str]:
    """The block ``name`` of a record's ``prefix`` markers."""
    return re.compile(
        rf"(<!-- {prefix}:{name}:start -->)(.*?)"
        rf"(<!-- {prefix}:{name}:end -->)",
        re.S,
    )


def cells(text: str) -> list[list[str]]:
    """A block's table rows as stripped cells; padding and dashes ignored."""
    rows = []
    for line in text.strip().splitlines():
        if not line.strip():
            continue
        row = [c.strip() for c in line.strip().strip("|").split("|")]
        rows.append([re.sub(r"^:?-+:?$", "---", c) for c in row])
    return rows


def main(
    record: Path,
    prefix: str,
    label: str,
    script: str,
    render: Callable[[], dict[str, str]],
    doc: str,
    argv: Sequence[str] | None = None,
) -> int:
    """Print, ``--write`` or ``--check`` one run's blocks in ``record``.

    ``render`` is called after the arguments parse, and returns every
    block's body by name; ``label`` names the run in the messages and
    ``script`` is what a stale block is told to run.
    """
    ap = argparse.ArgumentParser(description=doc.splitlines()[0])
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    a = ap.parse_args(argv)
    rendered = render()
    if not (a.write or a.check):
        for name, body in rendered.items():
            print(f"== {name}\n{body}\n")
        return 0
    text = record.read_text(encoding="utf-8")
    stale = []
    for name, body in rendered.items():
        m = span(prefix, name).search(text)
        if m is None:
            raise SystemExit(f"{label}: no {name} markers in {record.name}")
        if cells(m.group(2)) != cells(body):
            stale.append(name)
        text = span(prefix, name).sub(
            lambda m, b=body: m.group(1) + "\n\n" + b + "\n\n" + m.group(3),
            text,
        )
    if a.write:
        record.write_text(text, encoding="utf-8")
        print(
            f"{label}: wrote {len(rendered)} table block(s) in {record.name}"
        )
        return 0
    if stale:
        print(
            f"{label}: {record.name}'s {', '.join(stale)} table(s) are not "
            f"what the run's data renders. Run `python {script} --write`; "
            f"a cell is the data's output, never an edit."
        )
        return 1
    print(f"{label}: OK — {len(rendered)} table block(s) match the run's data")
    return 0
