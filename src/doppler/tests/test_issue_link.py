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

from doppler.tests._platform import skip_without_posix_shell
from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "issue-link-check.sh"


def _run(msg: str, tmp_path: Path) -> subprocess.CompletedProcess[str]:
    """Run the gate over one seeded commit message."""
    skip_without_posix_shell("scripts/issue-link-check.sh")
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


# ── A branch's commits must be its own ──────────────────────────────────────
#
# The declarations above are only this branch's if the commits carrying them
# are. `git commit --amend` after a hook-blocked commit rewrites HEAD, and on a
# fresh branch HEAD is the base's tip: the base commit returns under a new
# hash, with its author, author date and whole message -- its `No-issue:`
# included -- and the branch's work folded in. doppler#1472 was pushed that
# way and the gate PASSED it on a declaration the branch never made.
#
# These drive the real script over scratch repositories: the defect is about
# commit identity, which a seeded message file cannot carry. Author dates are
# pinned, because two commits in the same second by the same author with the
# same subject ARE indistinguishable from an amend -- a test that let the
# clock decide would pass or fail on timing.

_T0 = 1_700_000_000  # an arbitrary fixed epoch; each commit adds its own step


def _git(repo: Path, *args: str, when: int | None = None) -> str:
    env = None
    if when is not None:
        import os

        stamp = f"{when} +0000"
        env = {
            **os.environ,
            "GIT_AUTHOR_DATE": stamp,
            "GIT_COMMITTER_DATE": stamp,
        }
    r = subprocess.run(
        ["git", *args],
        cwd=repo,
        env=env,
        capture_output=True,
        text=True,
        check=True,
    )
    return r.stdout.strip()


def _commit(repo: Path, path: str, text: str, msg: str, when: int) -> str:
    f = repo / path
    f.parent.mkdir(parents=True, exist_ok=True)
    f.write_text(text)
    _git(repo, "add", "-A")
    _git(repo, "commit", "-q", "--no-verify", "-m", msg, when=when)
    return _git(repo, "rev-parse", "--short", "HEAD")


def _scratch(tmp_path: Path) -> Path:
    """`main` with two commits, the tip declaring `No-issue:` of its own."""
    repo = tmp_path / "repo"
    repo.mkdir()
    _git(repo, "init", "-q", "-b", "main")
    _git(repo, "config", "user.email", "t@example.com")
    _git(repo, "config", "user.name", "t")
    _commit(repo, "src/a.c", "int a;\n", "feat: a", _T0)
    _commit(
        repo,
        "docs/w.md",
        "w\n",
        "test(release): waivers\n\nNo-issue: enables the release",
        _T0 + 10,
    )
    return repo


def _gate(repo: Path, base: str = "main") -> subprocess.CompletedProcess[str]:
    skip_without_posix_shell("scripts/issue-link-check.sh")
    import os

    return subprocess.run(
        [str(SCRIPT)],
        cwd=repo,
        env={**os.environ, "ISSUE_BASE": base},
        capture_output=True,
        text=True,
    )


def test_an_amend_onto_the_base_tip_fails(tmp_path: Path) -> None:
    """The defect itself, exactly as #1472 hit it.

    The branch's work is committed with `--amend` onto the base's tip, so the
    branch holds that tip rewritten -- and its `No-issue:` would satisfy the
    declaration check below. The gate must refuse before it reads one, and
    name both hashes so the author can see which base commit was taken.
    """
    repo = _scratch(tmp_path)
    tip = _git(repo, "rev-parse", "--short", "main")
    _git(repo, "switch", "-q", "-c", "feat")
    (repo / "src" / "b.c").write_text("int b;\n")
    _git(repo, "add", "-A")
    _git(
        repo,
        "commit",
        "-q",
        "--no-verify",
        "--amend",
        "--no-edit",
        when=_T0 + 99,
    )
    mine = _git(repo, "rev-parse", "--short", "HEAD")
    r = _gate(repo)
    assert r.returncode == 1, r.stdout
    assert "rewritten" in r.stdout
    assert f"{mine} rewrites {tip}" in r.stdout, r.stdout


def test_the_same_amend_fails_in_a_ci_merge_checkout(tmp_path: Path) -> None:
    """CI does not check out the branch: it checks out a MERGE into the base.

    That merge's merge base is the base itself, so the rewritten original is
    not in `merge-base..base` -- a check that only searched what the base
    gained since the fork would see nothing there and pass. This is why the
    base's whole history is searched.
    """
    repo = _scratch(tmp_path)
    _git(repo, "switch", "-q", "-c", "feat")
    (repo / "src" / "b.c").write_text("int b;\n")
    _git(repo, "add", "-A")
    _git(
        repo,
        "commit",
        "-q",
        "--no-verify",
        "--amend",
        "--no-edit",
        when=_T0 + 99,
    )
    _git(repo, "switch", "-q", "--detach", "main")
    _git(repo, "merge", "-q", "--no-ff", "--no-edit", "feat", when=_T0 + 200)
    base = _git(repo, "rev-parse", "main")
    r = _gate(repo, base=base)
    assert r.returncode == 1, r.stdout
    assert "rewrites" in r.stdout


def test_a_reused_subject_on_a_new_commit_passes(tmp_path: Path) -> None:
    """The bot that repins the CI image uses one subject on every run.

    Its commit is new -- a new date -- so it is its own, and the gate must
    not confuse "the same words" with "the same commit". Matching on the
    subject alone would fail every repin PR.
    """
    repo = _scratch(tmp_path)
    _git(repo, "switch", "-q", "-c", "repin")
    _commit(
        repo,
        "src/pin.env",
        "x=2\n",
        "test(release): waivers\n\nNo-issue: routine",
        _T0 + 500,
    )
    r = _gate(repo)
    assert r.returncode == 0, r.stdout
    assert "rewrite" not in r.stdout


def test_a_branch_of_its_own_commits_passes(tmp_path: Path) -> None:
    """The ordinary branch: new commits, its own declaration."""
    repo = _scratch(tmp_path)
    _git(repo, "switch", "-q", "-c", "feat")
    _commit(repo, "src/b.c", "int b;\n", "feat: b\n\nNo-issue: none", _T0 + 50)
    r = _gate(repo)
    assert r.returncode == 0, r.stdout


def test_a_branch_whose_commit_already_merged_fails(tmp_path: Path) -> None:
    """The other way a branch holds a base commit under a second hash.

    doppler rebase-merges, so a merged commit lands on the base re-hashed but
    keeps its author and date. A branch still carrying the original -- a
    stacked child whose parent merged -- is not ready: the remedy is a
    rebase, and the failure names it.
    """
    repo = _scratch(tmp_path)
    _git(repo, "switch", "-q", "-c", "feat")
    mine = _commit(
        repo, "src/b.c", "int b;\n", "feat: b\n\nNo-issue: none", _T0 + 50
    )
    _git(repo, "switch", "-q", "main")
    _commit(repo, "src/c.c", "int c;\n", "feat: c", _T0 + 60)
    _git(repo, "cherry-pick", mine, when=_T0 + 70)
    _git(repo, "switch", "-q", "feat")
    r = _gate(repo)
    assert r.returncode == 1, r.stdout
    assert "git rebase main" in r.stdout
