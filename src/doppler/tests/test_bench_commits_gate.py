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

import importlib.util
import json
import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path
    from types import ModuleType

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
    assert f"{side[:9]} is not on main" in r.stdout


def test_a_stamp_not_in_history_fails(tmp_path: Path) -> None:
    repo, _ = _repo(tmp_path)
    _stamp(repo, "0123456789abcdef0123456789abcdef01234567")
    r = _gate(repo)
    assert r.returncode == 1, r.stdout + r.stderr
    assert "not in this repository's history" in r.stdout


def test_an_exempt_pair_passes(tmp_path: Path) -> None:
    repo, _ = _repo(tmp_path)
    side = _off_main(repo, "int b;\n")
    _stamp(repo, side)
    _exempt(repo, f"v1.0.0 {side[:9]} measured on a tree main never had\n")
    r = _gate(repo)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "1 exempt" in r.stdout


def test_an_exempt_set_does_not_excuse_a_new_file(tmp_path: Path) -> None:
    """Exempt is (set, commit): another off-main stamp in the set fails."""
    repo, _ = _repo(tmp_path)
    side = _off_main(repo, "int b;\n")
    _stamp(repo, side)
    _git(repo, "checkout", "-q", "side")
    other = _commit(repo, "int c;\n", "more off-main work")
    _git(repo, "checkout", "-q", "main")
    _stamp(repo, other, "new.json")
    _exempt(repo, f"v1.0.0 {side[:9]} measured on a tree main never had\n")
    r = _gate(repo)
    assert r.returncode == 1, r.stdout + r.stderr
    assert f"{other[:9]} is not on main" in r.stdout


def test_restamping_an_exempt_file_is_not_excused(tmp_path: Path) -> None:
    """The exempt pair goes stale and the new stamp fails."""
    repo, _ = _repo(tmp_path)
    side = _off_main(repo, "int b;\n")
    _git(repo, "checkout", "-q", "side")
    other = _commit(repo, "int c;\n", "more off-main work")
    _git(repo, "checkout", "-q", "main")
    _stamp(repo, other)  # was `side`, the exempt one
    _exempt(repo, f"v1.0.0 {side[:9]} measured on a tree main never had\n")
    r = _gate(repo)
    assert r.returncode == 1, r.stdout + r.stderr
    assert f"{other[:9]} is not on main" in r.stdout
    assert "no file there fails with that stamp" in r.stdout


def test_a_stale_exemption_fails(tmp_path: Path) -> None:
    """Sabotage (2): an exemption whose pair now passes must be deleted."""
    repo, base = _repo(tmp_path)
    _stamp(repo, base)
    _exempt(repo, f"v1.0.0 {base[:9]} once could not be fixed\n")
    r = _gate(repo)
    assert r.returncode == 1, r.stdout + r.stderr
    assert "no file there fails with that stamp -- delete the line" in (
        r.stdout
    )


def test_an_exemption_for_a_missing_set_is_stale(tmp_path: Path) -> None:
    repo, base = _repo(tmp_path)
    _stamp(repo, base)
    _exempt(repo, "v9.9.9 0123abcde no such release\n")
    r = _gate(repo)
    assert r.returncode == 1, r.stdout + r.stderr
    assert "directory is gone" in r.stdout


def test_an_exemption_needs_a_commit_and_a_reason(tmp_path: Path) -> None:
    repo, _ = _repo(tmp_path)
    side = _off_main(repo, "int b;\n")
    _stamp(repo, side)
    _exempt(repo, f"v1.0.0 {side[:9]}\n")
    r = _gate(repo)
    assert r.returncode == 1, r.stdout + r.stderr
    assert "needs `<release> <commit> <reason>`" in r.stdout


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
    assert "SHALLOW (1 commit(s) of history)" in r.stdout
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
    assert "if main is behind, fetch it first" in r.stdout
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


def _gate_on_main(repo: Path) -> None:
    """Commit a stand-in for the gate, so the ratchet is live at the base."""
    (repo / "scripts").mkdir(exist_ok=True)
    (repo / "scripts" / "check_bench_commits.py").write_text(
        "# the gate\n", encoding="utf-8"
    )


def test_an_added_exemption_fails(tmp_path: Path) -> None:
    """The list may only shrink: a pair the merge base lacks is refused."""
    repo, _ = _repo(tmp_path)
    old = _off_main(repo, "int old;\n")
    _stamp(repo, old)
    _gate_on_main(repo)
    _exempt(repo, f"v1.0.0 {old[:9]} measured on a tree main never had\n")
    _git(repo, "add", "-A")
    _git(repo, "commit", "-qm", "the gate and its honest exemption")
    _git(repo, "checkout", "-q", "-b", "feature")
    new = repo / "benchmarks/published/v2.0.0/native.json"
    new.parent.mkdir(parents=True)
    new.write_text(
        (repo / SET / "native.json").read_text(encoding="utf-8"),
        encoding="utf-8",
    )
    _exempt(
        repo,
        f"v1.0.0 {old[:9]} measured on a tree main never had\n"
        f"v2.0.0 {old[:9]} a new set, quietly excused\n",
    )
    r = _gate(repo)
    assert r.returncode == 1, r.stdout + r.stderr
    assert f"v2.0.0 {old[:9]} was ADDED" in r.stdout
    assert "v1.0.0" not in r.stdout.split("ADDED")[0].splitlines()[-1]


def test_a_list_absent_at_the_base_held_nothing(tmp_path: Path) -> None:
    """Deleting the list upstream must not switch the ratchet off.

    The gate is live at the base, the list is not there: every pair on
    the branch is ADDED. Before the shared helper, "absent" meant "new,
    skip" and this passed.
    """
    repo, _ = _repo(tmp_path)
    side = _off_main(repo, "int b;\n")
    _stamp(repo, side)
    _gate_on_main(repo)
    _git(repo, "add", "-A")
    _git(repo, "commit", "-qm", "the gate, and no exemptions")
    _git(repo, "checkout", "-q", "-b", "feature")
    _exempt(repo, f"v1.0.0 {side[:9]} excused on a branch\n")
    r = _gate(repo)
    assert r.returncode == 1, r.stdout + r.stderr
    assert f"v1.0.0 {side[:9]} was ADDED" in r.stdout


def test_a_commit_never_pushed_says_so(tmp_path: Path) -> None:
    """On no remote branch: fetching cannot help, so do not suggest it."""
    repo, _ = _repo(tmp_path)
    side = _off_main(repo, "int b;\n")
    _git(repo, "checkout", "-q", "side")
    _stamp(repo, side)
    r = _gate(repo)
    assert r.returncode == 1, r.stdout + r.stderr
    assert "on no remote branch: it was never pushed" in r.stdout
    # and the remedy list does not lead with a fetch that cannot help
    assert r.stdout.index("land it first") < r.stdout.index("git fetch")


def test_a_commit_on_a_remote_branch_is_told_to_fetch(
    tmp_path: Path,
) -> None:
    """On origin/feature but not origin/main: fetch is the first thing."""
    origin, _ = _repo(tmp_path)
    _git(tmp_path, "clone", "-q", f"file://{origin}", "clone")
    clone = tmp_path / "clone"
    _git(origin, "checkout", "-q", "-b", "feature")
    feature = _commit(origin, "int f;\n", "on a branch")
    _git(origin, "checkout", "-q", "main")
    _git(clone, "fetch", "-q", "origin")
    _stamp(clone, feature)
    r = _gate(clone, base="origin/main")
    assert r.returncode == 1, r.stdout + r.stderr
    assert "origin/feature has it -- if it has merged since, fetch" in (
        r.stdout
    )
    assert r.stdout.index("git fetch origin") < r.stdout.index("bench-restamp")


def test_the_verdict_names_a_missing_base(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """bench_interleaved asks before any gate runs: say "fetch"."""
    monkeypatch.syspath_prepend(str(REPO / "scripts"))
    spec = importlib.util.spec_from_file_location("_t_gate", GATE)
    assert spec is not None and spec.loader is not None
    gate = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(gate)
    repo, base = _repo(tmp_path)
    assert "origin/main is not here -- fetch it" in gate.verdict(
        repo, base, "origin/main"
    )


def test_not_a_git_checkout_says_so(tmp_path: Path) -> None:
    plain = tmp_path / "plain"
    (plain / SET).mkdir(parents=True)
    r = _gate(plain)
    assert r.returncode == 1, r.stdout + r.stderr
    assert "is not a git checkout" in r.stdout


def test_a_missing_base_ref_fails_both_scripts(tmp_path: Path) -> None:
    repo, base = _repo(tmp_path)
    _stamp(repo, base)
    r = _gate(repo, base="no-such-branch")
    assert r.returncode == 1, r.stdout + r.stderr
    assert "base ref no-such-branch not found" in r.stdout
    r = subprocess.run(
        [
            sys.executable,
            str(RESTAMP),
            "1.0.0",
            "--root",
            str(repo),
            "--base",
            "no-such-branch",
        ],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 1, r.stdout + r.stderr
    assert "base ref no-such-branch not found" in r.stdout


def test_restamp_fetches_a_stamp_it_does_not_have(tmp_path: Path) -> None:
    """The measured commit lives only on origin: fetched by commit_info.id."""
    origin, _ = _repo(tmp_path)
    _git(origin, "config", "uploadpack.allowAnySHA1InWant", "true")
    _git(tmp_path, "clone", "-q", f"file://{origin}", "clone")
    clone = tmp_path / "clone"
    side = _off_main(origin, "int measured;\n")  # made after the clone
    landed = _commit(clone, "int measured;\n", "the squash-merge")
    path = _stamp(clone, side)
    missing = subprocess.run(
        ["git", "-C", str(clone), "cat-file", "-e", f"{side}^{{commit}}"],
        capture_output=True,
    )
    assert missing.returncode != 0  # not local before the restamp

    r = _restamp(clone)
    assert r.returncode == 0, r.stdout + r.stderr
    after = json.loads(path.read_text(encoding="utf-8"))
    assert after["doppler_meta"]["commit"] == landed[:9]


# ── bench_restamp's guards, in-process ───────────────────────────────────


@pytest.fixture
def restamp(monkeypatch: pytest.MonkeyPatch) -> ModuleType:
    """bench_restamp as a module, its sibling imports resolvable."""
    monkeypatch.syspath_prepend(str(REPO / "scripts"))
    spec = importlib.util.spec_from_file_location("_t_restamp", RESTAMP)
    assert spec is not None and spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def test_the_rewrite_needs_exactly_one_match(restamp: ModuleType) -> None:
    text = json.dumps(
        {
            "other": {"commit": "abc123456"},
            "doppler_meta": {"commit": "abc123456"},
        }
    )
    with pytest.raises(restamp.RestampError, match="appears 2 times"):
        restamp._rewrite("x.json", text, "abc123456", "def987654")


def test_the_rewrite_is_parsed_back(restamp: ModuleType) -> None:
    """One textual match that is NOT doppler_meta.commit is refused.

    Here doppler_meta's value is spelled with a JSON escape, so the only
    plain-text match is another key's, and the parse-back catches the edit.
    """
    text = (
        '{"machine_info": {"commit": "abc123456"}, '
        '"doppler_meta": {"commit": "\\u0061bc123456"}}'
    )
    assert json.loads(text)["doppler_meta"]["commit"] == "abc123456"
    with pytest.raises(restamp.RestampError, match="changed more than"):
        restamp._rewrite("x.json", text, "abc123456", "def987654")


def test_an_ambiguous_abbreviation_is_refused(
    tmp_path: Path, restamp: ModuleType, monkeypatch: pytest.MonkeyPatch
) -> None:
    """If the new stamp at the old length names some OTHER commit, refuse.

    A real collision at nine hex digits cannot be built on demand, so the
    abbreviation's lookup is made to resolve elsewhere.
    """
    repo, _ = _repo(tmp_path)
    side = _off_main(repo, "int measured;\n")
    landed = _commit(repo, "int measured;\n", "the squash-merge")
    real = restamp.resolve

    def resolve(root: Path, ref: str) -> str | None:
        return "f" * 40 if ref == landed[:9] else real(root, ref)

    monkeypatch.setattr(restamp, "resolve", resolve)
    data = {"commit_info": {"id": side}, "doppler_meta": {"commit": side[:9]}}
    with pytest.raises(restamp.RestampError, match="ambiguous at 9"):
        restamp._target(repo, "x.json", data, "main")
