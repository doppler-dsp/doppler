"""The issue-link gate, exercised over seeded commit messages.

`scripts/issue-link-check.sh` takes message files as arguments precisely so
this file can drive its decision without fabricating a scratch repository —
the same reason `conflict-check.sh` is a script rather than an inline recipe.

The distinction under test is narrow and is the whole point: **mentioning an
issue is not closing one.** A branch whose message says ``See #714 for
context`` has told GitHub nothing, and GitHub will leave #714 open on merge.
That is exactly what happened to #714 itself — `c0e0e615` shipped the gate the
issue asked for and the issue stayed open for a day.
"""

from __future__ import annotations

import subprocess
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "issue-link-check.sh"


def _run(msg: str, tmp_path: Path) -> subprocess.CompletedProcess[str]:
    """Run the gate over one seeded commit message."""
    f = tmp_path / "msg.txt"
    f.write_text(msg)
    return subprocess.run(
        [str(SCRIPT), str(f)], capture_output=True, text=True, cwd=REPO
    )


ACCEPTED = [
    ("closes", "fix: a thing\n\nCloses #714\n"),
    ("Closes-capitalised", "fix: a thing\n\nCloses #714\n"),
    ("fixes", "fix: a thing\n\nFixes #714\n"),
    ("resolves", "fix: a thing\n\nResolves #714\n"),
    ("closed-past-tense", "fix: a thing\n\nClosed #714\n"),
    ("optout", "chore: re-vendor standard.mk\n\nNo-issue:\n"),
    ("multiple", "fix: two\n\nCloses #663\nCloses #664\n"),
]

REJECTED = [
    ("silent", "fix(docs): gate the generated C API tree\n\nSome body.\n"),
    ("bare-mention", "fix: a thing\n\nSee #714 for context.\n"),
    ("issue-number-only", "fix: a thing\n\n#714\n"),
    ("keyword-no-number", "fix: a thing\n\nCloses the gap.\n"),
]


@pytest.mark.parametrize(
    ("label", "msg"), ACCEPTED, ids=[c[0] for c in ACCEPTED]
)
def test_declared_branch_passes(tmp_path: Path, label: str, msg: str) -> None:
    """A branch that states what it closes -- or that it closes nothing."""
    r = _run(msg, tmp_path)
    assert r.returncode == 0, f"{label} was rejected:\n{r.stdout}"


@pytest.mark.parametrize(
    ("label", "msg"), REJECTED, ids=[c[0] for c in REJECTED]
)
def test_silent_branch_fails(tmp_path: Path, label: str, msg: str) -> None:
    """Silence, and every near-miss that reads like a link but is not one.

    ``#714`` alone and ``See #714`` both leave the issue open on merge, so
    they must fail for the same reason silence does. ``Closes the gap`` is the
    keyword without a number -- prose, not a link.
    """
    r = _run(msg, tmp_path)
    assert r.returncode == 1, f"{label} was accepted:\n{r.stdout}"
    assert "says nothing about" in r.stdout


def test_failure_names_both_remedies(tmp_path: Path) -> None:
    """The message has to be actionable -- both answers, not just the tidy one.

    A gate that only says "add Closes #N" pushes an author toward inventing a
    link for a branch that closes nothing, which is worse than silence.
    """
    r = _run("fix: a thing\n\nno declaration\n", tmp_path)
    assert "Closes #123" in r.stdout
    assert "No-issue:" in r.stdout


def test_make_lint_reaches_the_gate() -> None:
    """`make lint` runs it -- the half that is easy to get wrong.

    Two gates in this repo were correct and wired to nothing: a hook staged
    `pre-push` with no pre-push hook installed, and `changelog-check` listed
    only in GATES_DEPS while no CI job runs `make gates`.
    """
    r = subprocess.run(
        ["make", "-n", "issue-link-check"],
        capture_output=True,
        text=True,
        cwd=REPO,
    )
    assert r.returncode == 0, r.stderr
    assert "issue-link-check.sh" in r.stdout
