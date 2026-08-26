"""The guard that stops a branch gate passing when it cannot see the work.

`issue-link-check` and `changelog-check` both answer a question about the
diff against `origin/main`. Before the first commit there is no diff, so both
report `inert` and exit 0 — indistinguishable from a real verdict. Running
``make lint`` before committing is the natural order and is exactly when they
answer about nothing; #1012 went red in CI on a check that had passed locally
minutes earlier.

`scripts/uncommitted-code-guard.sh` is what makes that state loud. The three
states it distinguishes are the whole contract, so all three are pinned here
rather than the one that happens to be broken today.
"""

from __future__ import annotations

import subprocess
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
GUARD = REPO / "scripts" / "uncommitted-code-guard.sh"


def _repo(tmp_path: Path) -> Path:
    """A scratch git repo with one committed file under `src/`."""
    subprocess.run(["git", "init", "-q"], cwd=tmp_path, check=True)
    subprocess.run(
        ["git", "config", "user.email", "t@example.com"],
        cwd=tmp_path,
        check=True,
    )
    subprocess.run(
        ["git", "config", "user.name", "t"], cwd=tmp_path, check=True
    )
    (tmp_path / "src").mkdir()
    (tmp_path / "src" / "a.py").write_text("x = 1\n")
    subprocess.run(["git", "add", "-A"], cwd=tmp_path, check=True)
    subprocess.run(["git", "commit", "-qm", "seed"], cwd=tmp_path, check=True)
    return tmp_path


def _run(cwd: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(GUARD), *args], capture_output=True, text=True, cwd=cwd
    )


def test_clean_tree_passes_silently(tmp_path: Path) -> None:
    """Inert on a clean branch is honest: there is nothing to check."""
    r = _run(_repo(tmp_path), "--inert", "probe-gate", "src")
    assert r.returncode == 0, r.stdout + r.stderr
    assert r.stdout.strip() == "", "a clean tree must not print anything"


@pytest.mark.parametrize(
    ("label", "mutate"),
    [
        ("modified", lambda p: (p / "src" / "a.py").write_text("x = 2\n")),
        ("untracked", lambda p: (p / "src" / "b.py").write_text("y = 1\n")),
        (
            "staged",
            lambda p: (
                (p / "src" / "c.py").write_text("z = 1\n"),
                subprocess.run(["git", "add", "-A"], cwd=p, check=True),
            ),
        ),
    ],
)
def test_dirty_and_inert_fails(tmp_path: Path, label: str, mutate) -> None:
    """The trap. Work exists, the gate cannot read it, so it must not pass.

    All three spellings of "dirty" count. Untracked is the worst of them —
    a whole new object nobody committed — and is the one a `git diff`-based
    check would miss entirely.
    """
    repo = _repo(tmp_path)
    mutate(repo)
    r = _run(repo, "--inert", "probe-gate", "src")
    assert r.returncode != 0, f"{label}: guard passed on a dirty tree"
    assert "FAIL" in r.stdout
    assert "commit" in r.stdout.lower(), "the message must say what to do"


def test_dirty_without_inert_only_warns(tmp_path: Path) -> None:
    """With commits ahead the verdict is real, so this is a note, not a stop.

    Deliberate: failing here would fire on every mid-edit ``make lint``, and
    a gate that fires constantly is a gate someone switches off.
    """
    repo = _repo(tmp_path)
    (repo / "src" / "a.py").write_text("x = 2\n")
    r = _run(repo, "probe-gate", "src")
    assert r.returncode == 0, r.stdout + r.stderr
    assert "NOTE" in r.stdout


def test_paths_outside_the_code_roots_are_ignored(tmp_path: Path) -> None:
    """A dirty README is not uncommitted CODE, and must not block a gate."""
    repo = _repo(tmp_path)
    (repo / "README.md").write_text("hello\n")
    r = _run(repo, "--inert", "probe-gate", "src")
    assert r.returncode == 0, r.stdout + r.stderr


def test_makefile_call_sites_propagate_the_exit() -> None:
    """A recipe is one shell command; a non-zero exit mid-list aborts nothing.

    This is not hypothetical tidiness — it is the bug this guard shipped with.
    Wired into the Makefile it PRINTED its failure and the recipe carried on
    and exited 0, looking identical in the output to the script call site,
    which has `set -e` and aborted correctly. Every Makefile call site needs
    an explicit `|| exit 1`.
    """
    text = (REPO / "Makefile").read_text(encoding="utf-8")
    lines = text.splitlines()
    sites = [
        (i, ln)
        for i, ln in enumerate(lines)
        if "uncommitted-code-guard.sh" in ln and "--inert" in ln
    ]
    assert sites, "no --inert guard call site found in the Makefile"
    for i, ln in sites:
        # The call may wrap onto the next line with a trailing backslash.
        joined = ln
        j = i
        while joined.rstrip().endswith("\\") and j + 1 < len(lines):
            j += 1
            joined += " " + lines[j].strip()
        assert "|| exit 1" in joined, (
            f"Makefile:{i + 1} calls the guard without `|| exit 1`, so a "
            f"failure prints and the recipe still succeeds:\n    {ln.strip()}"
        )
