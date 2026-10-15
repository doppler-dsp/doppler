"""The bench box's preflight: a release's outputs go once main has them.

Each release is measured in a clone that keeps its snapshot untracked, and
`docs/benchmarks.md` modified, after the snapshot's PR merges. A clone per
release kept them all: 26 clones, about 10 GB, by 2026-10-14. So the box
keeps ONE clone, and `scripts/bench_interleaved.clear_published_copies` (run
by `make bench-preflight`, and first by `make bench-interleaved`) clears what
origin/main already has, compared as `git status` compares it, and refuses
over anything it lacks.

Each case is a scratch origin and a box clone, because what the preflight
decides is a relation between the clone and origin/main after a fetch. The
fixture's order is that of a real release: the box measures, and only then
does the snapshot PR merge on origin. So the box's origin/main lacks the
snapshot until the preflight fetches, and a preflight that skipped the
fetch would refuse the stale copy instead of clearing it.
"""

from __future__ import annotations

import importlib.util
import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path
    from types import ModuleType

SCRIPTS = repo_root(__file__) / "scripts"
SET = "benchmarks/published/v1.0.0"
PAGE = "docs/benchmarks.md"
SNAPSHOT = '{"benchmarks": [{"name": "bench_x"}]}\n'
RENDERED = "# Benchmarks\n\nv1.0.0: 1.5 us\n"


def _load() -> ModuleType:
    """Import scripts/bench_interleaved.py by path (scripts/ is no package)."""
    sys.path.insert(0, str(SCRIPTS))
    try:
        spec = importlib.util.spec_from_file_location(
            "bench_interleaved", SCRIPTS / "bench_interleaved.py"
        )
        assert spec and spec.loader
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod
    finally:
        sys.path.remove(str(SCRIPTS))


def _git(cwd: Path, *args: str) -> str:
    return subprocess.run(
        ["git", "-c", "user.email=t@t", "-c", "user.name=t", *args],
        cwd=cwd,
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()


def _write(repo: Path, rel: str, text: str) -> None:
    """Exactly these bytes: a text-mode write is CRLF on Windows, and the
    tests are about the gate, not the platform."""
    path = repo / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(text.encode("utf-8"))


@pytest.fixture
def box(tmp_path: Path) -> Path:
    """The bench box's clone, at the release commit it measured.

    origin/main then gains the v1.0.0 snapshot and its rendered page (the
    snapshot PR merging), which the clone has not fetched yet.
    """
    origin = tmp_path / "origin.git"
    _git(tmp_path, "init", "-q", "--bare", "-b", "main", str(origin))
    dev = tmp_path / "dev"
    _git(tmp_path, "clone", "-q", str(origin), str(dev))
    _write(dev, PAGE, "# Benchmarks\n")
    _git(dev, "add", "-A")
    _git(dev, "commit", "-qm", "release commit")
    _git(dev, "push", "-q", "origin", "HEAD:main")

    clone = tmp_path / "box"
    _git(tmp_path, "clone", "-q", str(origin), str(clone))

    _write(dev, f"{SET}/native.json", SNAPSHOT)
    _write(dev, PAGE, RENDERED)
    _git(dev, "add", "-A")
    _git(dev, "commit", "-qm", "docs: publish benchmarks for v1.0.0")
    _git(dev, "push", "-q", "origin", "HEAD:main")
    return clone


def _measured(clone: Path, snapshot: str = SNAPSHOT) -> None:
    """What the box's run left: its snapshot, and the page it rendered."""
    _write(clone, f"{SET}/native.json", snapshot)
    _write(clone, PAGE, RENDERED)


def _preflight(clone: Path) -> int:
    return _load().clear_published_copies("origin/main", root=str(clone))


def test_a_copy_main_has_is_cleared_and_the_checkout_then_works(
    box: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    _measured(box)
    assert _preflight(box) == 0
    out = capsys.readouterr().out
    assert f"cleared {SET}/" in out
    assert f"cleared {PAGE}" in out
    assert not (box / SET).exists()
    assert _git(box, "status", "--porcelain") == ""
    # The point of clearing it: the next release commit checks out.
    _git(box, "checkout", "-q", "--detach", "origin/main")


def test_a_crlf_copy_of_mains_file_is_cleared_under_autocrlf(
    box: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """On a Windows checkout (`core.autocrlf=true`) a copy of main's file
    arrives CRLF. Git calls it the same content, and so must the preflight.
    Hashed as raw bytes, every such copy "differed", and the gate refused
    everything on Windows. Safe, and useless there."""
    _git(box, "config", "core.autocrlf", "true")
    _write(box, f"{SET}/native.json", SNAPSHOT.replace("\n", "\r\n"))
    _write(box, PAGE, RENDERED.replace("\n", "\r\n"))
    assert _preflight(box) == 0
    out = capsys.readouterr().out
    assert f"cleared {SET}/" in out and f"cleared {PAGE}" in out
    assert not (box / SET).exists()
    assert _git(box, "status", "--porcelain") == ""


def test_a_snapshot_that_differs_from_mains_refuses(
    box: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    _measured(box, snapshot='{"benchmarks": []}\n')
    assert _preflight(box) == 2
    out = capsys.readouterr().out
    assert f"{SET}/native.json differs from origin/main's" in out
    assert "Publish each through its PR, or remove it" in out
    assert (box / f"{SET}/native.json").exists()


def test_a_snapshot_main_lacks_refuses(
    box: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    _write(box, "benchmarks/published/v2.0.0/native.json", SNAPSHOT)
    assert _preflight(box) == 2
    out = capsys.readouterr().out
    assert "v2.0.0/native.json is not on origin/main" in out
    assert (box / "benchmarks/published/v2.0.0/native.json").exists()


def test_a_tracked_page_that_differs_from_mains_refuses(
    box: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    _write(box, PAGE, "# Benchmarks\n\nre-rendered by hand\n")
    assert _preflight(box) == 2
    assert f"{PAGE} differs from origin/main's" in capsys.readouterr().out
    assert "by hand" in (box / PAGE).read_text(encoding="utf-8")


def test_a_refusal_clears_nothing_not_even_the_stale_copy(
    box: Path,
) -> None:
    """All or nothing, so the run that refuses leaves the clone as it was."""
    _measured(box)
    _write(box, "benchmarks/published/v2.0.0/native.json", SNAPSHOT)
    assert _preflight(box) == 2
    assert (box / f"{SET}/native.json").exists()
    assert (box / PAGE).read_text(encoding="utf-8") == RENDERED


def test_a_staged_snapshot_refuses_even_one_main_has(
    box: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """Staged, a file is neither untracked nor different from the index,
    and `git checkout` carries it into the next release's commit."""
    _write(box, f"{SET}/native.json", SNAPSHOT)  # main's bytes, exactly
    _git(box, "add", SET)
    assert _preflight(box) == 2
    assert f"{SET}/native.json is staged and not committed" in (
        capsys.readouterr().out
    )
    assert _git(box, "diff", "--cached", "--name-only") == f"{SET}/native.json"


@pytest.mark.parametrize(
    "target", ["missing.json", "."], ids=["dangling", "dir"]
)
def test_a_symlink_refuses_by_name_and_touches_nothing(
    box: Path, target: str, capsys: pytest.CaptureFixture[str]
) -> None:
    link = box / SET / "native.json"
    link.parent.mkdir(parents=True)
    link.symlink_to(target)
    _write(box, PAGE, RENDERED)  # stale, and must survive the refusal
    assert _preflight(box) == 2
    assert f"{SET}/native.json is a symlink" in capsys.readouterr().out
    assert link.is_symlink()
    assert (box / PAGE).read_text(encoding="utf-8") == RENDERED


def test_a_failed_fetch_refuses(
    box: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """Without the fetch, origin/main is the clone's stale view of it, and
    whether the copy is published cannot be known."""
    _measured(box)
    _git(box, "remote", "set-url", "origin", str(box / "no-such-origin"))
    assert _preflight(box) == 2
    assert "`git fetch origin` failed" in capsys.readouterr().out
    assert (box / f"{SET}/native.json").exists()


def test_bench_preflight_is_the_scripts_own_mode(
    box: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """`make bench-preflight` runs `bench_interleaved.py --preflight`."""
    inter = _load()
    monkeypatch.setattr(inter, "REPO", str(box))
    monkeypatch.setattr(sys, "argv", ["bench_interleaved.py", "--preflight"])
    _measured(box)
    assert inter.main() == 0
    assert not (box / SET).exists()


def test_bench_interleaved_runs_it_before_anything_is_measured(
    box: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """The refusal comes before the machine check and the hour of
    building, so an unpublished result is never measured over."""
    inter = _load()
    monkeypatch.setattr(inter, "REPO", str(box))
    monkeypatch.delenv("MAKEFLAGS", raising=False)
    monkeypatch.delenv("BENCH_ARGS", raising=False)

    def too_late() -> list[str]:
        raise AssertionError("reached the machine check past the preflight")

    monkeypatch.setattr(inter, "machine_not_ready", too_late)
    monkeypatch.setattr(sys, "argv", ["bench_interleaved.py", "2.0.0"])
    _write(box, "benchmarks/published/v2.0.0/native.json", SNAPSHOT)
    assert inter.main() == 2
    assert "not on origin/main" in capsys.readouterr().out
