#!/usr/bin/env python3
"""Gate: a retired word stays out of the prose it is retired from.

``docs/design/vocabulary.md``, under the heading "Retired words", is the one
declaration: two tables headed ``| retired | use instead | allowed |``, one
row per retired word or phrase. They are found by that heading text (any
level, any number prefix) and by that column header, never by a section
number or a position on the page, so the page can be renumbered or
restructured without the gate losing them; a missing heading or table fails.

This reads them and holds the prose to them, so a rename the page declares
is not finished when the build goes green but when the old word appears
nowhere -- the gap ``check_retired_names.py`` closes for identifiers, closed
here for words.

**What a row says** (the page's own paragraph is the contract; this restates
only what it reads):

* ``retired`` lists phrases, each in backticks, matched as WHOLE WORDS without
  regard to case. A parenthetical in ``retired`` is the sense the row is
  about, and is not matched. A phrase is matched across a line break, since
  prose is hard-wrapped.
* ``allowed`` lists phrases, each in backticks, inside which a match is not a
  use; ``—`` means none.
* A row whose ``allowed`` is ``review only`` is skipped here: it is a common
  word with other senses in this tree, which review holds.

**Which prose.** Every hand-written page ``_docs_pages.pages()`` returns --
``docs/**/*.md`` less ``docs/c-api``, ``archive/`` and build trees -- less the
measurement records (``*-measurements.md``), and within a page less fenced
blocks, code spans and generated regions (``_docs_pages.prose``). The first
table applies to all of it, the second to ``docs/design/*.md`` only (a page
states what is, so its prose does not narrate). Headers, docstrings and
``objects/*.toml`` are not scanned; that carve-out is a declared follow-up.

**Existing hits** are listed in ``scripts/.retired-words-exempt`` as
``<page> <count> <phrase>``. The match is exact both ways, so the list can
only SHRINK: more hits than listed is a new use of a retired word, fewer is a
stale entry that must be lowered or deleted, and an entry naming a phrase the
tables no longer script is itself a failure. The exemptions cannot outlive
their reason, and a page already listed cannot take a new use quietly.

Nothing is registered: the pages are globbed and the rows come out of the
vocabulary page, so a new page or a new row is covered by existing.

Usage:  python3 scripts/check_retired_words.py [--root DIR]
Exit 0 when every hit is listed and every listed hit is still there.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path

from _docs_pages import pages, prose

ROOT = Path(__file__).resolve().parent.parent

#: The declaration the rows are read from.
VOCAB = Path("docs") / "design" / "vocabulary.md"

#: Existing hits, `<page> <count> <phrase>` per line; the list only shrinks.
EXEMPT = Path("scripts") / ".retired-words-exempt"

#: The "Retired words" heading, at any level and with any number prefix
#: (`## 4. Retired words`, `### 3.2 Retired words`, `# Retired words`).
_HEADING = re.compile(
    r"^(#{1,6})[ \t]+(?:\d+(?:\.\d+)*\.?[ \t]+)?Retired words[ \t]*$", re.M
)
_ANY_HEADING = re.compile(r"^(#{1,6})[ \t]+\S", re.M)
_HEADER = re.compile(
    r"^\|\s*retired\s*\|\s*use instead\s*\|\s*allowed\s*\|\s*$", re.I
)
_PARENTHETICAL = re.compile(r"\([^)]*\)")
_PHRASE = re.compile(r"`([^`]+)`")


@dataclass(frozen=True)
class Row:
    """One scripted row: a phrase, what to use instead, where it applies."""

    phrase: str
    use: str
    allowed: tuple[str, ...]
    design_only: bool


def _cells(line: str) -> list[str]:
    """The cells of a table row, split on unescaped pipes."""
    parts = re.split(r"(?<!\\)\|", line.strip())
    return [c.strip() for c in parts[1:-1]]


def _section(text: str) -> str | None:
    """The text under the "Retired words" heading, up to the next heading of
    the same or a higher level (a sub-heading stays inside), or None."""
    m = _HEADING.search(text)
    if not m:
        return None
    level = len(m.group(1))
    tail = text[m.end() :]
    for n in _ANY_HEADING.finditer(tail):
        if len(n.group(1)) <= level:
            return tail[: n.start()]
    return tail


def read_rows(text: str) -> tuple[list[Row], int, bool]:
    """The scripted rows under the "Retired words" heading, how many tables
    it holds, and whether the heading was found at all.

    A row is scripted unless its ``allowed`` cell is ``review only``. The
    first table applies to all prose, the second to design pages only.
    """
    section = _section(text)
    if section is None:
        return [], 0, False
    lines = section.splitlines()
    rows: list[Row] = []
    tables = 0
    i = 0
    while i < len(lines):
        if not _HEADER.match(lines[i].strip()):
            i += 1
            continue
        tables += 1
        i += 2  # the header and its separator
        while i < len(lines) and lines[i].lstrip().startswith("|"):
            cells = _cells(lines[i])
            i += 1
            if len(cells) != 3 or cells[2].strip().lower() == "review only":
                continue
            retired, use, allowed = cells
            for phrase in _PHRASE.findall(_PARENTHETICAL.sub("", retired)):
                rows.append(
                    Row(
                        phrase=phrase,
                        use=use,
                        allowed=tuple(_PHRASE.findall(allowed)),
                        # the page's own contract, not a position on the
                        # page: of the two tables under the heading, the
                        # first applies to all prose, the second to design
                        # pages only
                        design_only=tables == 2,
                    )
                )
    return rows, tables, True


def _pattern(phrase: str) -> re.Pattern[str]:
    """A phrase as a whole-word, case-insensitive pattern that tolerates the
    line break a hard-wrapped page puts between its words."""
    words = r"\s+".join(re.escape(w) for w in phrase.split())
    return re.compile(rf"(?<!\w){words}(?!\w)", re.I)


def scan_page(
    text: str, rows: list[Row], design: bool
) -> list[tuple[int, Row]]:
    """(line, row) of every use of a retired phrase in a page's prose."""
    body = prose(text)
    hits: list[tuple[int, Row]] = []
    for row in rows:
        if row.design_only and not design:
            continue
        allowed = [
            m.span() for a in row.allowed for m in _pattern(a).finditer(body)
        ]
        for m in _pattern(row.phrase).finditer(body):
            if any(lo <= m.start() and m.end() <= hi for lo, hi in allowed):
                continue
            hits.append((body.count("\n", 0, m.start()) + 1, row))
    return hits


def read_exempt(root: Path) -> dict[tuple[str, str], int]:
    """``(page, phrase) -> count`` from the exempt file."""
    out: dict[tuple[str, str], int] = {}
    path = root / EXEMPT
    if not path.exists():
        return out
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError as e:
        raise SystemExit(
            f"{EXEMPT.as_posix()}: not valid UTF-8 ({e.reason} at byte "
            f"{e.start}); save it as UTF-8"
        ) from e
    for n, line in enumerate(text.splitlines(), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split(None, 2)
        if len(parts) != 3 or not parts[1].isdigit():
            raise SystemExit(
                f"{EXEMPT.as_posix()}:{n}: expected `<page> <count> <phrase>`"
            )
        out[(parts[0], parts[2].lower())] = int(parts[1])
    return out


def _is_design(rel: str) -> bool:
    parts = Path(rel).parts
    return len(parts) == 3 and parts[:2] == ("docs", "design")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--root",
        type=Path,
        default=ROOT,
        help="tree to scan; the gate's own test points this at a seeded one",
    )
    root = ap.parse_args(argv).root.resolve()

    vocab = root / VOCAB
    if not vocab.is_file():
        print(
            f"check_retired_words: FAIL — {VOCAB.as_posix()} is missing; "
            "the rows are read from it",
            file=sys.stderr,
        )
        return 1
    try:
        vocab_text = vocab.read_text(encoding="utf-8")
    except UnicodeDecodeError as e:
        print(
            f"check_retired_words: FAIL — {VOCAB.as_posix()} is not valid "
            f"UTF-8 ({e.reason} at byte {e.start}); the rows are read as "
            "UTF-8, so save it as UTF-8",
            file=sys.stderr,
        )
        return 1
    rows, tables, found = read_rows(vocab_text)
    scanned = [
        p for p in pages(root) if not p.name.endswith("-measurements.md")
    ]
    if not found:
        print(
            f"check_retired_words: FAIL — {VOCAB.as_posix()} has no "
            '"Retired words" heading; the rows are read from under it',
            file=sys.stderr,
        )
        return 1
    if tables != 2 or not rows or not scanned:
        print(
            "check_retired_words: FAIL — read "
            f"{tables} table(s), {len(rows)} scripted phrase(s) and "
            f"{len(scanned)} page(s); a scan over nothing did not run, so it "
            'has not passed (the "Retired words" heading holds two tables)',
            file=sys.stderr,
        )
        return 1

    exempt = read_exempt(root)
    scripted = {r.phrase.lower() for r in rows}
    failures: list[str] = []
    seen: Counter[tuple[str, str]] = Counter()
    where: dict[tuple[str, str], list[tuple[int, Row]]] = {}
    for page in scanned:
        rel = page.relative_to(root).as_posix()
        text = page.read_text(encoding="utf-8", errors="replace")
        for line, row in scan_page(text, rows, _is_design(rel)):
            key = (rel, row.phrase.lower())
            seen[key] += 1
            where.setdefault(key, []).append((line, row))

    for key, n in sorted(seen.items()):
        listed = exempt.get(key, 0)
        if n == listed:
            continue
        page, _ = key
        if n > listed:
            for line, row in where[key]:
                scope = " (design pages)" if row.design_only else ""
                failures.append(
                    f"{page}:{line}: `{row.phrase}` is retired{scope}; use: "
                    f"{row.use}"
                )
            if listed:
                failures.append(
                    f"{page}: {n} use(s) of `{key[1]}` against {listed} "
                    f"listed in {EXEMPT.as_posix()}; the list only shrinks, "
                    "so a new use is fixed, not listed."
                )
        else:
            failures.append(
                f"{EXEMPT.as_posix()}: {page} has {n} use(s) of `{key[1]}`, "
                f"{listed} listed; lower the count to {n} (the list only "
                "shrinks)."
            )
    for (page, phrase), listed in sorted(exempt.items()):
        if (page, phrase) in seen:
            continue
        if phrase not in scripted:
            failures.append(
                f"{EXEMPT.as_posix()}: `{phrase}` is not a scripted row of "
                f"{VOCAB.as_posix()}; delete `{page} {listed} {phrase}`."
            )
        else:
            failures.append(
                f"{EXEMPT.as_posix()}: {page} no longer uses `{phrase}`; "
                f"delete `{page} {listed} {phrase}` (the list only shrinks)."
            )

    if failures:
        print("check_retired_words: FAIL\n", file=sys.stderr)
        for f in failures:
            print(f"  - {f}", file=sys.stderr)
        return 1

    print(
        f"check_retired_words: OK — {len(scripted)} scripted phrase(s) across "
        f"{len(scanned)} page(s); {sum(exempt.values())} existing use(s) "
        f"listed in {len(exempt)} exempt entr(y/ies)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
