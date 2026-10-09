"""The bench-commits gate and bench-restamp, over throwaway repositories.

A published benchmark set stamps `doppler_meta.commit`, and a reader must
be able to check that commit out. `scripts/check_bench_commits.py` refuses a
stamp that main cannot reach, and `scripts/bench_restamp.py` moves a stamp
onto the one commit on main with the identical tree (#1322).

Each case builds a small repository, because what both scripts decide is a
property of a commit graph: an ancestor of main or not, a tree shared by
zero, one or two commits on main, a clone that is shallow. The real tree
can only show the cases it happens to contain. Here every branch is built
on purpose, including the ones a release has not produced yet.
"""

from __future__ import annotations

import json
import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
GATE = REPO / "scripts" / "check_bench_commits.py"
RESTAMP = REPO / "scripts" / "bench_restamp.py"
SET = "benchmarks/published/v1.0.0"


def _git(cwd: Path, *args: str) -> str:
    return subprocess.run(
        ["git", "-c", "user.email=t@t", "-c", "user.name=t", *args],
        cwd=cwd,
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()


def _commit(repo: Path, content: str, msg: str) -> str:
    """Commit ``content`` as the whole of ``code.c``; return the SHA."""
    (repo / "code.c").write_text(content, encoding="utf-8")
    _git(repo, "add", "code.c")
    _git(repo, "commit", "-qm", msg)
    return _git(repo, "rev-parse", "HEAD")


def _stamp(repo: Path, sha: str, name: str = "native.json") -> Path:
    """A snapshot whose doppler_meta.commit is ``sha`` cut to 9 chars."""
    path = repo / SET / name
    path.parent.mkdir(parents=True, exist_ok=True)
    data = {
        "machine_info": {"node": "box"},
        "commit_info": {"id": sha, "dirty": False},
        "benchmarks": [{"name": "bench_x", "stats": {"mean": 1.5e-6}}],
        "doppler_meta": {"measured_at": "2026-10-09", "commit": sha[:9]},
    }
    path.write_text(json.dumps(data, indent=1), encoding="utf-8")
    return path


def _repo(tmp_path: Path) -> tuple[Path, str]:
    """A repository on main with one commit; returns (repo, its SHA)."""
    repo = tmp_path / "repo"
    repo.mkdir()
    _git(repo, "init", "-q", "-b", "main")
    return repo, _commit(repo, "int a;\n", "base")


def _off_main(repo: Path, content: str) -> str:
    """A commit on a side branch that main never receives."""
    _git(repo, "checkout", "-q", "-b", "side")
    sha = _commit(repo, content, "measured on the release branch")
    _git(repo, "checkout", "-q", "main")
    return sha


def _gate(repo: Path, base: str = "main") -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(GATE), "--root", str(repo), "--base", base],
        capture_output=True,
        text=True,
    )


def _restamp(repo: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            str(RESTAMP),
            "1.0.0",
            "--root",
            str(repo),
            "--base",
            "main",
        ],
        capture_output=True,
        text=True,
    )


def _exempt(repo: Path, body: str) -> None:
    (repo / "scripts").mkdir(exist_ok=True)
    (repo / "scripts" / ".bench-commit-exempt").write_text(
        body, encoding="utf-8"
    )


# ── the gate ─────────────────────────────────────────────────────────────


def test_a_stamp_on_main_passes(tmp_path: Path) -> None:
    repo, base = _repo(tmp_path)
    _stamp(repo, base)
    r = _gate(repo)
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_stamp_off_main_fails(tmp_path: Path) -> None:
    """Sabotage (1): a set measured on a branch main never received."""
    repo, _ = _repo(tmp_path)
    side = _off_main(repo, "int b;\n")
    _stamp(repo, side)
    r = _gate(repo)
    assert r.returncode == 1, r.stdout + r.stderr
    assert f"{side[:9]} is not an ancestor of main" in r.stdout


def test_a_stamp_not_in_history_fails(tmp_path: Path) -> None:
    repo, _ = _repo(tmp_path)
    _stamp(repo, "0123456789abcdef0123456789abcdef01234567")
    r = _gate(repo)
    assert r.returncode == 1, r.stdout + r.stderr
    assert "not in this repository's history" in r.stdout


def test_an_exempt_set_passes(tmp_path: Path) -> None:
    repo, _ = _repo(tmp_path)
    _stamp(repo, _off_main(repo, "int b;\n"))
    _exempt(repo, "v1.0.0 measured on a tree main never had\n")
    r = _gate(repo)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "1 exempt" in r.stdout


def test_a_stale_exemption_fails(tmp_path: Path) -> None:
    """Sabotage (2): an exemption whose set now passes must be deleted."""
    repo, base = _repo(tmp_path)
    _stamp(repo, base)
    _exempt(repo, "v1.0.0 once could not be fixed\n")
    r = _gate(repo)
    assert r.returncode == 1, r.stdout + r.stderr
    assert "now passes -- delete the line" in r.stdout


def test_an_exemption_for_a_missing_set_is_stale(tmp_path: Path) -> None:
    repo, base = _repo(tmp_path)
    _stamp(repo, base)
    _exempt(repo, "v9.9.9 no such release\n")
    r = _gate(repo)
    assert r.returncode == 1, r.stdout + r.stderr
    assert "directory is gone" in r.stdout


def test_an_exemption_needs_a_reason(tmp_path: Path) -> None:
    repo, _ = _repo(tmp_path)
    _stamp(repo, _off_main(repo, "int b;\n"))
    _exempt(repo, "v1.0.0\n")
    r = _gate(repo)
    assert r.returncode == 1, r.stdout + r.stderr
    assert "has no reason" in r.stdout


def test_a_shallow_clone_names_the_depth(tmp_path: Path) -> None:
    """Sabotage (3): shallow history cannot tell off-main from unfetched,
    so the gate fails on the depth, not on the snapshot."""
    repo, base = _repo(tmp_path)
    _commit(repo, "int c;\n", "later")
    _stamp(repo, base)
    _git(repo, "add", "-A")
    _git(repo, "commit", "-qm", "publish")
    shallow = tmp_path / "shallow"
    _git(tmp_path, "clone", "-q", "--depth", "1", f"file://{repo}", "shallow")
    r = _gate(shallow, base="origin/main")
    assert r.returncode == 1, r.stdout + r.stderr
    assert "SHALLOW" in r.stdout
    assert "fetch-depth: 0" in r.stdout


# ── bench-restamp ────────────────────────────────────────────────────────


def test_restamp_moves_to_the_one_tree_identical_commit(
    tmp_path: Path,
) -> None:
    repo, _ = _repo(tmp_path)
    side = _off_main(repo, "int measured;\n")
    landed = _commit(repo, "int measured;\n", "the squash-merge")
    path = _stamp(repo, side)
    before = json.loads(path.read_text(encoding="utf-8"))

    r = _restamp(repo)
    assert r.returncode == 0, r.stdout + r.stderr
    after = json.loads(path.read_text(encoding="utf-8"))
    assert after["doppler_meta"]["commit"] == landed[:9]
    assert after["commit_info"]["id"] == side  # the literal checkout stays
    before["doppler_meta"]["commit"] = landed[:9]
    assert after == before  # and nothing else moved
    assert _gate(repo).returncode == 0

    again = _restamp(repo)  # idempotent: already on main
    assert again.returncode == 0, again.stdout + again.stderr
    assert "already names commits on main" in again.stdout


def test_restamp_refuses_when_no_commit_on_main_has_the_tree(
    tmp_path: Path,
) -> None:
    """Sabotage (4a): the measured content never landed as-is."""
    repo, _ = _repo(tmp_path)
    side = _off_main(repo, "int measured;\n")
    path = _stamp(repo, side)
    text = path.read_text(encoding="utf-8")

    r = _restamp(repo)
    assert r.returncode == 1, r.stdout + r.stderr
    assert "0 commit(s) on main have it (none)" in r.stdout
    assert path.read_text(encoding="utf-8") == text  # nothing written


def test_restamp_refuses_when_two_commits_on_main_have_the_tree(
    tmp_path: Path,
) -> None:
    """Sabotage (4b): two candidates, so the provenance is ambiguous."""
    repo, _ = _repo(tmp_path)
    side = _off_main(repo, "int measured;\n")
    first = _commit(repo, "int measured;\n", "landed")
    _commit(repo, "int other;\n", "changed")
    second = _commit(repo, "int measured;\n", "reverted")
    path = _stamp(repo, side)
    text = path.read_text(encoding="utf-8")

    r = _restamp(repo)
    assert r.returncode == 1, r.stdout + r.stderr
    assert "2 commit(s) on main have it" in r.stdout
    assert first[:12] in r.stdout and second[:12] in r.stdout
    assert path.read_text(encoding="utf-8") == text


def test_restamp_writes_nothing_if_any_file_is_refused(
    tmp_path: Path,
) -> None:
    """A set is restamped whole or not at all."""
    repo, _ = _repo(tmp_path)
    good = _off_main(repo, "int measured;\n")
    _commit(repo, "int measured;\n", "landed")
    _git(repo, "checkout", "-q", "side")
    bad = _commit(repo, "int never_landed;\n", "more branch work")
    _git(repo, "checkout", "-q", "main")
    a = _stamp(repo, good, "a.json")
    b = _stamp(repo, bad, "b.json")
    texts = (a.read_text(encoding="utf-8"), b.read_text(encoding="utf-8"))

    r = _restamp(repo)
    assert r.returncode == 1, r.stdout + r.stderr
    assert "nothing written" in r.stdout
    assert (
        a.read_text(encoding="utf-8"),
        b.read_text(encoding="utf-8"),
    ) == texts
