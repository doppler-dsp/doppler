"""Render a measurement record's tables from committed data, and check them.

A record under ``docs/design/`` that quotes a run keeps the run's data
beside it and a script that renders every one of its tables between
markers::

    <!-- <prefix>:<block>:start -->

    | ... |

    <!-- <prefix>:<block>:end -->

A run may keep blocks on more than one page (``main_pages``): a design
page that quotes a record's numbers holds them as blocks of the same run.
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


def _sync(
    record: Path, prefix: str, label: str, rendered: dict[str, str]
) -> tuple[str, list[str]]:
    """``record``'s text with every block replaced by its rendering, and the
    names of the blocks whose cells differed."""
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
    return text, stale


def main_pages(
    prefix: str,
    label: str,
    script: str,
    render: Callable[[], dict[Path, dict[str, str]]],
    doc: str,
    argv: Sequence[str] | None = None,
) -> int:
    """Print, ``--write`` or ``--check`` one run's blocks across pages.

    ``render`` is called after the arguments parse and returns, per page,
    every block's body by name: a run whose numbers a design page quotes
    keeps them there as blocks too, so a re-run cannot leave the page
    stale. ``label`` names the run in the messages and ``script`` is what a
    stale block is told to run.
    """
    ap = argparse.ArgumentParser(description=doc.splitlines()[0])
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    a = ap.parse_args(argv)
    pages = render()
    n = sum(len(r) for r in pages.values())
    if not (a.write or a.check):
        for page, rendered in pages.items():
            for name, body in rendered.items():
                print(f"== {page.name}: {name}\n{body}\n")
        return 0
    stale: list[str] = []
    texts = {}
    for page, rendered in pages.items():
        texts[page], bad = _sync(page, prefix, label, rendered)
        stale += [f"{page.name}'s {b}" for b in bad]
    if a.write:
        for page, text in texts.items():
            page.write_text(text, encoding="utf-8")
        names = ", ".join(p.name for p in pages)
        print(f"{label}: wrote {n} table block(s) in {names}")
        return 0
    if stale:
        print(
            f"{label}: {', '.join(stale)} table(s) are not what the run's "
            f"data renders. Run `python {script} --write`; a cell is the "
            f"data's output, never an edit."
        )
        return 1
    print(f"{label}: OK — {n} table block(s) match the run's data")
    return 0


def main(
    record: Path,
    prefix: str,
    label: str,
    script: str,
    render: Callable[[], dict[str, str]],
    doc: str,
    argv: Sequence[str] | None = None,
) -> int:
    """``main_pages`` for a run that keeps its blocks on one page."""
    return main_pages(
        prefix, label, script, lambda: {record: render()}, doc, argv
    )
