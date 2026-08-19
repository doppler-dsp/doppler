"""The conflict-marker gate, exercised over seeded files.

`scripts/conflict-check.sh` exists as a script rather than as an inline
Makefile recipe precisely so this file can drive it: a lint target whose only
exercise is corrupting the repository is a target nobody ever proves, so it
rots silently and is discovered the way upstream discovered theirs — by a
conflict reaching a published docs site.

Each marker form gets its own case because they fail differently. The raw
three are what git writes; the other two are what **mdformat** leaves behind
after `make format` has passed over a conflicted markdown file, and they are
the reason a check written against the literal three is not enough:

============================  ==========================================
in the conflict               after mdformat
============================  ==========================================
``<<<<<<< HEAD``              ``\\<<\\<<\\<<< HEAD`` — every ``<`` escaped
``=======``                   **gone** — a line of ``=`` under text is a
                              setext H1, so the sentence above is promoted
                              to a heading and lands in the page's ToC
``>>>>>>> d19e3ae (...)``     ``> > > > > > > d19e3ae`` — nested quotes
============================  ==========================================

The ``=======`` row is why this gate has to run on the way *in*: once
mdformat has rewritten it there is nothing left to search for.
"""

from __future__ import annotations

import subprocess
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "conflict-check.sh"


def _run(*args: str | Path) -> subprocess.CompletedProcess[str]:
    """Run the gate on explicit paths, capturing both streams."""
    return subprocess.run(
        [str(SCRIPT), *map(str, args)],
        capture_output=True,
        text=True,
        cwd=REPO,
    )


# The five forms the gate must reject: the three git writes, plus the two
# mdformat leaves behind. `=======` is listed bare because that is exactly how
# git writes it -- a trailing space would be a different line.
REJECTED = pytest.mark.parametrize(
    ("label", "line"),
    [
        ("raw-ours", "<<<<<<< HEAD"),
        ("raw-sep", "======="),
        ("raw-theirs", ">>>>>>> d19e3ae (some commit subject)"),
        ("mdformat-ours", r"\<<\<<\<<< HEAD"),
        ("mdformat-theirs", "> > > > > > > d19e3ae (some commit subject)"),
    ],
    ids=lambda v: v if isinstance(v, str) and "-" in v else "",
)


@REJECTED
def test_marker_is_rejected(tmp_path: Path, label: str, line: str) -> None:
    """Every marker form fails the gate and is named in the output."""
    f = tmp_path / "seeded.md"
    f.write_text(f"before\n{line}\nafter\n")

    r = _run(f)

    assert r.returncode == 1, f"{label} was not rejected:\n{r.stdout}"
    assert "merge-conflict marker" in r.stdout
    assert "seeded.md" in r.stdout


def test_clean_file_passes(tmp_path: Path) -> None:
    """A file with no markers passes, so the gate is not vacuously red."""
    f = tmp_path / "clean.md"
    f.write_text("# Title\n\nOrdinary prose, no markers.\n")

    r = _run(f)

    assert r.returncode == 0, r.stdout
    assert "no conflict markers" in r.stdout


def test_indented_marker_passes(tmp_path: Path) -> None:
    """A marker inside a fenced code block is documentation, not a conflict.

    The gate is anchored at column 1 for this reason: git never writes an
    indented marker, and this repo's own prose quotes them when explaining
    exactly this class of bug -- including the module docstring above.
    """
    f = tmp_path / "doc.md"
    f.write_text("Explaining a conflict:\n\n```\n    <<<<<<< HEAD\n```\n")

    r = _run(f)

    assert r.returncode == 0, r.stdout


def test_tracked_tree_is_clean() -> None:
    """The real repository carries no markers, in any of the five forms.

    Run with no arguments, which is the mode `make lint-conflict` uses: the
    script walks `git ls-files` itself rather than trusting a caller's list.
    """
    r = _run()

    assert r.returncode == 0, r.stdout


def test_make_lint_reaches_the_gate() -> None:
    """`make lint` actually runs it -- the half that is easy to get wrong.

    A gate can be correct and wired to nothing; that is how both of this
    repo's previously-dead gates failed (a hook staged `pre-push` with no
    pre-push hook installed, and a target listed only in `GATES_DEPS` while no
    CI job runs `make gates`). Asserting the wiring is cheap.
    """
    r = subprocess.run(
        ["make", "-n", "lint-conflict"],
        capture_output=True,
        text=True,
        cwd=REPO,
    )

    assert r.returncode == 0, r.stderr
    assert "conflict-check.sh" in r.stdout
