"""The retired-words gate, exercised over a seeded tree.

`scripts/check_retired_words.py` reads the two tables of
`docs/design/vocabulary.md`, under its "Retired words" heading, and holds
the docs prose to them. The tree seeded here carries its OWN copy of the
tables (so the test does not move when a live row does), and every defect
the gate was written for must be caught: a retired phrase in prose, wrapped
across a line, in its plural when the row lists the plural, in a different
case, and a design-only phrase on a design page.

So must every shape it must NOT flag, because a gate that fires on them gets
turned off: a phrase in a code span, a fenced block, a generated region, a
measurement record, generated C API and an archived page; a phrase inside an
`allowed` phrase; the sense text of a parenthetical; a `review only` row; a
design-only phrase on a page that is not a design page; a longer word that
merely contains the phrase.

And the exempt list may only shrink, exactly both ways: more uses than listed
is a new use, fewer is a stale entry, and an entry for a phrase the tables no
longer script is a finding. A tree with no table, or the wrong number of them,
has not been scanned, so it has not passed. The message of a finding carries
the page and line, the phrase, and the row's "use instead".
"""

from __future__ import annotations

import os
import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

SCRIPT = repo_root(__file__) / "scripts" / "check_retired_words.py"

VOCAB = """\
# Vocabulary

## 4. Retired words

Prose the rows below mention in backticks is not a use.

| retired        | use instead        | allowed           |
| -------------- | ------------------ | ----------------- |
| `block lock`   | ratio-of-sums lock | `block lock-free` |
| `tile`, `tiles` | Doppler roll      | —                 |
| `look count`   | non-coherent count | —                 |
| `frobnicate` (for the quux of a loop) | smooth | —      |
| `block` (for a lock's decision interval) | count | review only |

On design pages only, narration.

| retired                    | use instead | allowed     |
| -------------------------- | ----------- | ----------- |
| `as built`                 | the present | —           |
| `today` (against a before) | the present | review only |

## 5. Names

Not read by the gate: `block lock` appears here and nowhere it counts.
"""


def _tree(
    tmp_path: Path,
    pages: dict[str, str],
    exempt: str = "",
    vocab: str = VOCAB,
) -> Path:
    root = tmp_path / "repo"
    files = {
        "docs/design/vocabulary.md": vocab,
        "scripts/.retired-words-exempt": exempt,
        "docs/index.md": "# Index\n\nNothing retired here.\n",
        **pages,
    }
    for rel, text in files.items():
        (root / rel).parent.mkdir(parents=True, exist_ok=True)
        # UTF-8, not the platform's default: the vocabulary's `allowed`
        # column is an em dash, which Windows' cp1252 writes as byte 0x97
        (root / rel).write_text(text, encoding="utf-8")
    return root


#: The script prints an em dash; UTF-8 on both ends, whatever the platform's
#: default (Windows' is cp1252, a console's may be anything).
_ENV = {**os.environ, "PYTHONIOENCODING": "utf-8"}


def _run(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root)],
        capture_output=True,
        encoding="utf-8",
        env=_ENV,
        check=False,
    )


def test_the_repository_passes():
    r = subprocess.run(
        [sys.executable, str(SCRIPT)],
        capture_output=True,
        encoding="utf-8",
        env=_ENV,
        check=False,
    )
    assert r.returncode == 0, r.stderr


def test_a_retired_phrase_in_prose_is_found_with_its_page_line_and_fix(
    tmp_path,
):
    root = _tree(
        tmp_path,
        {"docs/guide/g.md": "# G\n\nThe block lock declares.\n"},
    )
    r = _run(root)
    assert r.returncode == 1
    assert "docs/guide/g.md:3" in r.stderr
    assert "`block lock` is retired" in r.stderr
    assert "ratio-of-sums lock" in r.stderr  # the use-instead text


def test_a_phrase_wrapped_across_a_line_break_is_found_at_its_first_word(
    tmp_path,
):
    root = _tree(
        tmp_path,
        {"docs/guide/g.md": "# G\n\nThe look\ncount is eight.\n"},
    )
    r = _run(root)
    assert r.returncode == 1
    assert "docs/guide/g.md:3" in r.stderr


def test_the_match_ignores_case_and_a_listed_plural_is_a_row_of_its_own(
    tmp_path,
):
    root = _tree(
        tmp_path,
        {"docs/guide/g.md": "# G\n\nBLOCK Lock here, and two tiles there.\n"},
    )
    r = _run(root)
    assert r.returncode == 1
    assert "`block lock` is retired" in r.stderr
    assert "`tiles` is retired" in r.stderr


def test_a_design_only_phrase_is_red_on_a_design_page_and_green_elsewhere(
    tmp_path,
):
    design = _tree(
        tmp_path, {"docs/design/d.md": "# D\n\nAs built, it works.\n"}
    )
    r = _run(design)
    assert r.returncode == 1
    assert "docs/design/d.md:3" in r.stderr
    assert "`as built` is retired (design pages)" in r.stderr
    assert "the present" in r.stderr

    other = _tree(
        tmp_path / "other",
        {
            "docs/guide/g.md": "# G\n\nAs built, it works.\n",
            "docs/design/sub/deep.md": "# Deep\n\nAs built, it works.\n",
        },
    )
    assert _run(other).returncode == 0


def test_every_shape_that_is_not_prose_or_not_a_use_passes(tmp_path):
    good = (
        "# Good\n\n"
        "In a span `block lock` and `look count` are named.\n\n"
        "```text\nblock lock and the tiles\n```\n\n"
        "<!-- gen:x:start -->\nthe block lock, generated\n"
        "<!-- gen:x:end -->\n\n"
        "The block lock-free path is an allowed phrase.\n"
        "A quux of a loop is the sense, not the word: frobnicating a quux.\n"
        "The block of a lock stays review only; so does the word block.\n"
        "A preblock lock is another word; blocks lock too.\n"
    )
    root = _tree(
        tmp_path,
        {
            "docs/guide/good.md": good,
            "docs/design/x-measurements.md": "# M\n\nblock lock, tile.\n",
            "docs/c-api/gen.md": "# C\n\nblock lock\n",
            "docs/archive/old.md": "# Old\n\nblock lock\n",
        },
    )
    r = _run(root)
    assert r.returncode == 0, r.stderr


def test_the_sense_in_a_parenthetical_is_not_matched_but_the_phrase_is(
    tmp_path,
):
    root = _tree(
        tmp_path,
        {"docs/guide/g.md": "# G\n\nFrobnicate the loop; a quux stays.\n"},
    )
    r = _run(root)
    assert r.returncode == 1
    assert "`frobnicate` is retired" in r.stderr
    assert "quux" not in r.stderr.replace("quux of a loop", "")


def test_an_exempt_entry_holds_its_count_exactly(tmp_path):
    page = "docs/guide/g.md"
    text = "# G\n\nThe block lock. Another block lock.\n"
    ok = _tree(tmp_path, {page: text}, exempt=f"{page} 2 block lock\n")
    assert _run(ok).returncode == 0

    # more uses than listed: a new use, not an exemption
    more = _tree(
        tmp_path / "more", {page: text}, exempt=f"{page} 1 block lock\n"
    )
    r = _run(more)
    assert r.returncode == 1
    assert "the list only shrinks" in r.stderr

    # the entry deleted while its uses remain
    gone = _tree(tmp_path / "gone", {page: text}, exempt="")
    assert _run(gone).returncode == 1


def test_an_exempt_entry_that_is_stale_or_unscripted_is_a_finding(tmp_path):
    page = "docs/guide/g.md"
    clean = "# G\n\nNothing retired here.\n"
    stale = _tree(tmp_path, {page: clean}, exempt=f"{page} 1 block lock\n")
    r = _run(stale)
    assert r.returncode == 1
    assert "no longer uses `block lock`" in r.stderr

    fewer = _tree(
        tmp_path / "fewer",
        {page: "# G\n\nThe block lock once.\n"},
        exempt=f"{page} 3 block lock\n",
    )
    r = _run(fewer)
    assert r.returncode == 1
    assert "lower the count to 1" in r.stderr

    unscripted = _tree(
        tmp_path / "unscripted",
        {page: clean},
        exempt=f"{page} 1 block\n",
    )
    r = _run(unscripted)
    assert r.returncode == 1
    assert "not a scripted row" in r.stderr


def test_the_tables_are_found_by_their_heading_not_their_number(tmp_path):
    page = {"docs/guide/g.md": "# G\n\nThe block lock declares.\n"}
    for heading in (
        "# Retired words",
        "### 3.2 Retired words",
        "## 12. Retired words",
        "## Retired words  ",
    ):
        root = _tree(
            tmp_path / heading.strip("# .0123456789").replace(" ", "_"),
            page,
            vocab=VOCAB.replace("## 4. Retired words", heading),
        )
        r = _run(root)
        assert r.returncode == 1, heading
        assert "`block lock` is retired" in r.stderr, heading

    # a sub-heading inside the section does not end it
    sub = _tree(
        tmp_path / "sub",
        page,
        vocab=VOCAB.replace(
            "On design pages only, narration.",
            "### Design pages\n\nOn design pages only, narration.",
        ),
    )
    r = _run(sub)
    assert r.returncode == 1
    assert "`block lock` is retired" in r.stderr


def test_a_missing_heading_or_table_is_red_not_green(tmp_path):
    clean = {"docs/guide/g.md": "# G\n\nNothing retired here.\n"}
    # the tables are all there, under a heading that is not "Retired words"
    no_heading = _tree(
        tmp_path,
        clean,
        vocab=VOCAB.replace("## 4. Retired words", "## 4. Old words"),
    )
    r = _run(no_heading)
    assert r.returncode == 1
    assert 'no "Retired words" heading' in r.stderr

    # the heading, and the second table moved out from under it
    moved = _tree(
        tmp_path / "moved",
        clean,
        vocab=VOCAB.replace(
            "On design pages only, narration.",
            "## 5. Other\n\nOn design pages only, narration.",
        ),
    )
    r = _run(moved)
    assert r.returncode == 1
    assert "1 table(s)" in r.stderr


def test_a_scan_over_nothing_has_not_passed(tmp_path):
    no_page = _tree(tmp_path, {})
    (no_page / "docs/design/vocabulary.md").unlink()
    r = _run(no_page)
    assert r.returncode == 1
    assert "is missing" in r.stderr

    one_table = _tree(
        tmp_path / "one",
        {},
        vocab=VOCAB.split("On design pages only")[0] + "\n## 5. Names\n",
    )
    r = _run(one_table)
    assert r.returncode == 1
    assert "a scan over nothing" in r.stderr


def test_a_vocabulary_or_exempt_file_that_is_not_utf8_is_a_named_failure(
    tmp_path,
):
    """Windows' default encoding (cp1252) writes the table's em dash as byte
    0x97, which is not UTF-8. The rows are read as UTF-8, so such a page is
    refused by name, with how to fix it, and not with a stack trace."""
    root = _tree(tmp_path, {})
    (root / "docs/design/vocabulary.md").write_bytes(VOCAB.encode("cp1252"))
    r = _run(root)
    assert r.returncode == 1
    assert "docs/design/vocabulary.md is not valid UTF-8" in r.stderr
    assert "save it as UTF-8" in r.stderr
    assert "Traceback" not in r.stderr

    exempt = _tree(tmp_path / "exempt", {})
    (exempt / "scripts/.retired-words-exempt").write_bytes(
        "# \u2014 a dash\n".encode("cp1252")
    )
    r = _run(exempt)
    assert r.returncode == 1
    assert ".retired-words-exempt: not valid UTF-8" in r.stderr
    assert "Traceback" not in r.stderr
