"""The section-citation gate, exercised over a seeded tree.

`scripts/check_doc_sections.py` checks that a `docs/x.md §N` citation names a
section that is really there. It shipped without a test, and the shape of what
it missed is the argument for having one: it recognised exactly one spelling —
a repository-relative path with `§N` directly after it — and printed "OK, 135
citations resolve" while sixty more sat unexamined beside it. A gate that
reports a number over a set it silently chose is worse than one that never ran,
because the number is believed.

That hole is what let the `async-dsss-receiver.md` split go unnoticed: §12
moved to the companion measurements page, and the citations left pointing at
the old one were found by grep. Widening the gate to all three spellings then
found three more the same day, in `CHANGELOG.md`, aimed at two design pages
folded into `mpsk.md` a month earlier.

Which files are scanned was the same defect one level up: the scan named seven
directories, `native/benchmarks` was not among them, and a benchmark's citation
to a section that had moved was checked by nobody. It surfaced only by
sabotaging a real citation and watching the gate stay green — the one way an
inclusion list's gaps are ever found. The scan set is now git's.

So each spelling is seeded here and must be caught: path-first, a
continuation sharing the previous citation's document, and a markdown link
whose path follows the number and is relative to the citing file. The two
things that must NOT be
caught are seeded too — a page's own `§4.1 and §4.2` self-reference names no
document, and a title that matches is not a finding — because a gate that fires
on those is one someone turns off.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_doc_sections.py"

#: The cited page. Numbered headings, and one the citations get wrong.
TARGET = """\
# The widget

## 1. What it is

Prose.

## 3.2 The NDA discriminator + lock signal

Prose.

## 8. The shapes

Prose.
"""


def _run(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root)],
        capture_output=True,
        text=True,
        cwd=REPO,
    )


def _tree(tmp_path: Path, citing: str) -> Path:
    """A seeded tree: one cited page under `docs/design`, one citing file."""
    design = tmp_path / "docs" / "design"
    design.mkdir(parents=True)
    (design / "widget.md").write_text(TARGET, encoding="utf-8")
    (tmp_path / "CHANGELOG.md").write_text(citing, encoding="utf-8")
    return tmp_path


# --------------------------------------------------------------------------
# Each spelling resolves when it is right, and is caught when it is wrong.
# --------------------------------------------------------------------------

GOOD = (
    ("path-first", "See `docs/design/widget.md` §3.2 for the derivation.\n"),
    (
        "continuation-comma",
        "See docs/design/widget.md §1, §3.2 and §8 for the derivation.\n",
    ),
    (
        "continuation-slash",
        "See docs/design/widget.md §3.2/§8 for the derivation.\n",
    ),
    (
        "markdown-link-repo-relative",
        "See [design §3.2](docs/design/widget.md) for the derivation.\n",
    ),
    (
        "markdown-link-anchored",
        "See [design §8](docs/design/widget.md#the-shapes) for it.\n",
    ),
    (
        "titled",
        'See `docs/design/widget.md` §3.2, "The NDA discriminator '
        '+ lock signal".\n',
    ),
    (
        "section-word",
        "See docs/design/widget.md section 3.2 for the derivation.\n",
    ),
    (
        "wrapped-before-the-mark",
        "/* The derivation (docs/design/widget.md\n * §3.2) holds. */\n",
    ),
    (
        "wrapped-before-the-number",
        "/* The derivation (docs/design/widget.md section\n * 3.2). */\n",
    ),
)

BAD = (
    ("path-first", "See `docs/design/widget.md` §4.9 for the derivation.\n"),
    (
        "continuation-comma",
        "See docs/design/widget.md §3.2, §4.9 for the derivation.\n",
    ),
    (
        "continuation-and",
        "See docs/design/widget.md §3.2 and §4.9 for the derivation.\n",
    ),
    (
        "continuation-slash",
        "See docs/design/widget.md §3.2/§4.9 for the derivation.\n",
    ),
    (
        "markdown-link-repo-relative",
        "See [design §4.9](docs/design/widget.md) for the derivation.\n",
    ),
    (
        "markdown-link-two-in-one-label",
        "See [the widget's §3.2/§4.9](docs/design/widget.md) for it.\n",
    ),
    ("deleted-page", "See `docs/design/gone.md` §5 for the derivation.\n"),
    (
        "section-word",
        "See docs/design/widget.md section 4.9 for the derivation.\n",
    ),
    (
        "wrapped-before-the-mark",
        "/* The derivation (docs/design/widget.md\n * §4.9) holds. */\n",
    ),
    (
        "wrapped-before-the-number",
        "/* The derivation (docs/design/widget.md section\n * 4.9). */\n",
    ),
    (
        "wrapped-hash",
        "# The derivation (docs/design/widget.md\n# §4.9) holds.\n",
    ),
    (
        "wrapped-slashes",
        "// The derivation (docs/design/widget.md\n// §4.9) holds.\n",
    ),
    (
        "capitalised-section",
        "See docs/design/widget.md Section 4.9 for the derivation.\n",
    ),
)


@pytest.mark.parametrize(("shape", "text"), GOOD, ids=[s for s, _ in GOOD])
def test_a_citation_that_resolves_passes(
    tmp_path: Path, shape: str, text: str
) -> None:
    proc = _run(_tree(tmp_path, text))
    assert proc.returncode == 0, proc.stdout + proc.stderr


@pytest.mark.parametrize(("shape", "text"), BAD, ids=[s for s, _ in BAD])
def test_a_citation_that_does_not_resolve_is_caught(
    tmp_path: Path, shape: str, text: str
) -> None:
    """Sabotage, one spelling at a time. Each must go red."""
    proc = _run(_tree(tmp_path, text))
    assert proc.returncode == 1, proc.stdout + proc.stderr
    assert "points somewhere it should not" in proc.stdout


# --------------------------------------------------------------------------
# What must NOT be a finding.
# --------------------------------------------------------------------------


def test_a_page_citing_its_own_sections_is_not_claimed(tmp_path: Path) -> None:
    """`§4.1 and §4.2` names no document, so it is nobody's to check.

    Without the joiner rule the continuation would inherit whatever document
    was last named anywhere in the file, and every self-reference in
    `docs/design/` would become a false finding — which is how a gate gets
    switched off rather than fixed.
    """
    body = (
        "The budget has two terms (`docs/design/widget.md` §3.2).\n"
        "\n"
        "Later, about this very page: see §4.1 and §4.2 below.\n"
    )
    proc = _run(_tree(tmp_path, body))
    assert proc.returncode == 0, proc.stdout + proc.stderr


def test_a_continuation_does_not_cross_prose(tmp_path: Path) -> None:
    """Only a joiner carries the document forward, and only a short one."""
    body = (
        "See `docs/design/widget.md` §3.2 for the derivation. A different\n"
        "matter entirely is handled in §4.9 of some other document.\n"
    )
    proc = _run(_tree(tmp_path, body))
    assert proc.returncode == 0, proc.stdout + proc.stderr


def test_a_wrong_title_is_caught(tmp_path: Path) -> None:
    """The half that matters: §3.2 exists, but it is about something else."""
    body = 'See `docs/design/widget.md` §3.2, "The invariant".\n'
    proc = _run(_tree(tmp_path, body))
    assert proc.returncode == 1, proc.stdout
    assert "the number or the title is" in proc.stdout


def test_a_relative_link_resolves_against_the_citing_file(
    tmp_path: Path,
) -> None:
    """`docs/design/*.md` cite each other by bare filename.

    A repository-relative prefix match would report every one of them as a
    missing page, so the resolution has to be relative to the citer.
    """
    design = tmp_path / "docs" / "design"
    design.mkdir(parents=True)
    (design / "widget.md").write_text(TARGET, encoding="utf-8")
    (design / "peer.md").write_text(
        "# Peer\n\nAs [the widget §3.2](widget.md) has it.\n", encoding="utf-8"
    )
    proc = _run(tmp_path)
    assert proc.returncode == 0, proc.stdout + proc.stderr


def test_a_tracked_file_anywhere_is_scanned(tmp_path: Path) -> None:
    """The scan set is git's, so no directory has to be remembered.

    Seeded in a real checkout, and in `native/benchmarks` specifically:
    that directory was outside the inclusion list this replaced, so the
    citation living there went unchecked while the gate reported a count
    that did not include it.
    """
    design = tmp_path / "docs" / "design"
    design.mkdir(parents=True)
    (design / "widget.md").write_text(TARGET, encoding="utf-8")
    bench = tmp_path / "native" / "benchmarks"
    bench.mkdir(parents=True)
    (bench / "bench_widget.c").write_text(
        "/* The operating point (docs/design/widget.md §4.9). */\n",
        encoding="utf-8",
    )
    for cmd in (
        ["git", "init", "-q"],
        ["git", "add", "-A"],
    ):
        subprocess.run(cmd, cwd=tmp_path, check=True, capture_output=True)

    proc = _run(tmp_path)
    assert proc.returncode == 1, proc.stdout + proc.stderr
    assert "native/benchmarks/bench_widget.c" in proc.stdout


def test_any_tracked_text_file_is_scanned(tmp_path: Path) -> None:
    """Not a suffix list: `native/validation/CMakeLists.txt` cited seven
    sections of a page that no longer had them while the scan read only
    `.h .c .md .py`."""
    design = tmp_path / "docs" / "design"
    design.mkdir(parents=True)
    (design / "widget.md").write_text(TARGET, encoding="utf-8")
    (tmp_path / "CMakeLists.txt").write_text(
        "# The harness (docs/design/widget.md §4.9).\n", encoding="utf-8"
    )
    for cmd in (["git", "init", "-q"], ["git", "add", "-A"]):
        subprocess.run(cmd, cwd=tmp_path, check=True, capture_output=True)
    proc = _run(tmp_path)
    assert proc.returncode == 1, proc.stdout + proc.stderr
    assert "CMakeLists.txt:1" in proc.stdout


@pytest.mark.parametrize(
    "text",
    [
        "As the design §3.2 has it.\n",
        "As design section 3.2 has it.\n",
        "/* As the design\n * section 3.2 has it. */\n",
        "Design section 3.2 has it.\n",
    ],
    ids=["mark", "word", "wrapped", "capitalised"],
)
def test_a_citation_naming_no_page_is_refused(
    tmp_path: Path, text: str
) -> None:
    """`design §3.2` names no page, so nothing can check it: refused unless
    its file is on the shrink-only list."""
    proc = _run(_tree(tmp_path, text))
    assert proc.returncode == 1, proc.stdout + proc.stderr
    assert "without naming the page" in proc.stdout


def test_a_link_label_names_its_page(tmp_path: Path) -> None:
    """`[design §3.2](widget.md)` names the page in the link: not bare."""
    body = "See [design §3.2](docs/design/widget.md) for it.\n"
    proc = _run(_tree(tmp_path, body))
    assert proc.returncode == 0, proc.stdout + proc.stderr


def _exempt(root: Path, line: str) -> None:
    path = root / "scripts" / ".doc-section-bare-exempt"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("# the ratchet\n" + line, encoding="utf-8")


def test_the_list_admits_what_existed_and_only_shrinks(tmp_path: Path) -> None:
    """One listed, one present: passes. A second one added: refused. The
    one removed and the entry left at 1: refused, until it is lowered."""
    root = _tree(tmp_path, "As the design §3.2 has it.\n")
    _exempt(root, "1 CHANGELOG.md\n")
    assert _run(root).returncode == 0

    (root / "CHANGELOG.md").write_text(
        "As the design §3.2 has it, and design §8 too.\n", encoding="utf-8"
    )
    grew = _run(root)
    assert grew.returncode == 1, grew.stdout
    assert "2 citation(s) say `design §N`" in grew.stdout

    (root / "CHANGELOG.md").write_text(
        "As docs/design/widget.md §3.2 has it.\n", encoding="utf-8"
    )
    stale = _run(root)
    assert stale.returncode == 1, stale.stdout
    assert "lower its entry" in stale.stdout


def test_an_empty_scan_fails(tmp_path: Path) -> None:
    """A scan that matched nothing has not run, so it has not passed."""
    (tmp_path / "empty").mkdir()
    proc = _run(tmp_path)
    assert proc.returncode == 1, proc.stdout


def test_the_repository_passes() -> None:
    """The gate over the real tree, which is what CI runs."""
    proc = subprocess.run(
        [sys.executable, str(SCRIPT)],
        capture_output=True,
        text=True,
        cwd=REPO,
    )
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert "section citation(s) resolve" in proc.stdout
