"""The repin commit guard and the repin gate's fetch, in scratch repositories.

Two halves of one misdiagnosis (2026-10-01). The nightly's ci/repin-image
commit bc37092f was reported as a PARENTLESS snapshot that would revert
main. It was an ordinary one-file child of main. What made it look orphaned
was ``ci-image-repin-check.sh``'s ``git fetch --depth=1``: in a full clone,
that writes the fetched commit into ``.git/shallow`` as a graft boundary,
after which ``rev-list`` and ``merge-base`` see no parent.

- ``ci-image-repin-commit-check.sh`` is what ci-image.yml now asserts before
  it pushes: one file, on exactly the run's commit. It reads the commit
  object, so a graft cannot fool it.
- The gate's fetch must leave a full clone full.
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
GUARD = REPO / "scripts" / "ci-image-repin-commit-check.sh"
GATE = REPO / "scripts" / "ci-image-repin-check.sh"
PIN = ".github/ci-images.env"
BLOCK = "CI_IMAGE_FINGERPRINT_2204={f}\nCI_IMAGE_FINGERPRINT_2404=b\n"
BLOCK += "CI_IMAGE_SOURCE_HASH=c\n"


def _git(cwd: Path, *args: str) -> str:
    return subprocess.run(
        ["git", "-c", "user.name=t", "-c", "user.email=t@t", *args],
        cwd=cwd,
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()


def _repo(tmp_path: Path) -> tuple[Path, str]:
    """A repository with one base commit carrying a pin; returns (dir, sha)."""
    skip_without_posix_shell("the CI-image repin scripts")
    r = tmp_path / "r"
    (r / ".github").mkdir(parents=True)
    _git(tmp_path, "init", "-q", "-b", "main", str(r))
    (r / PIN).write_text(BLOCK.format(f="a"))
    (r / "src.c").write_text("int x;\n")
    _git(r, "add", "-A")
    _git(r, "commit", "-q", "-m", "base")
    return r, _git(r, "rev-parse", "HEAD")


def _guard(r: Path, base: str, head: str = "HEAD"):
    return subprocess.run(
        ["sh", str(GUARD), base, head], cwd=r, capture_output=True, text=True
    )


def test_a_one_file_repin_passes(tmp_path: Path) -> None:
    r, base = _repo(tmp_path)
    (r / PIN).write_text(BLOCK.format(f="z"))
    _git(r, "commit", "-qam", "repin")
    g = _guard(r, base)
    assert g.returncode == 0, g.stdout


def test_a_repin_touching_another_file_fails(tmp_path: Path) -> None:
    r, base = _repo(tmp_path)
    (r / PIN).write_text(BLOCK.format(f="z"))
    (r / "src.c").write_text("int y;\n")
    _git(r, "commit", "-qam", "repin plus a stray edit")
    g = _guard(r, base)
    assert g.returncode == 1
    assert "touches more than" in g.stdout
    assert "src.c" in g.stdout


def test_an_orphan_repin_fails(tmp_path: Path) -> None:
    """The shape #1510 described: the whole tree, with no parent."""
    r, base = _repo(tmp_path)
    _git(r, "checkout", "-q", "--orphan", "snap")
    (r / PIN).write_text(BLOCK.format(f="z"))
    _git(r, "add", "-A")
    _git(r, "commit", "-q", "-m", "snapshot")
    g = _guard(r, base)
    assert g.returncode == 1
    assert "0 parent(s)" in g.stdout


def test_a_repin_on_another_base_fails(tmp_path: Path) -> None:
    r, base = _repo(tmp_path)
    (r / "src.c").write_text("int y;\n")
    _git(r, "commit", "-qam", "main moved")
    (r / PIN).write_text(BLOCK.format(f="z"))
    _git(r, "commit", "-qam", "repin on the moved main")
    g = _guard(r, base)
    assert g.returncode == 1
    assert "not the base" in g.stdout


@pytest.mark.parametrize("pending", [True, False], ids=["pending", "none"])
def test_the_gate_leaves_a_full_clone_full(
    tmp_path: Path, pending: bool
) -> None:
    """Before the fix, this fetch made a full clone shallow.

    An origin with main and (optionally) ci/repin-image one commit ahead; a
    FULL clone of it; the gate run in that clone. Afterwards the clone must
    not be shallow, and the repin must still show its parent.
    """
    origin, _ = _repo(tmp_path)
    if pending:
        _git(origin, "checkout", "-q", "-b", "ci/repin-image")
        (origin / PIN).write_text(BLOCK.format(f="z"))
        _git(origin, "commit", "-qam", "repin")
        _git(origin, "checkout", "-q", "main")
    clone = tmp_path / "clone"
    _git(tmp_path, "clone", "-q", f"file://{origin}", str(clone))
    assert _git(clone, "rev-parse", "--is-shallow-repository") == "false"

    g = subprocess.run(
        ["sh", str(GATE)], cwd=clone, capture_output=True, text=True
    )
    assert g.returncode == (1 if pending else 0), g.stdout + g.stderr

    assert _git(clone, "rev-parse", "--is-shallow-repository") == "false"
    if pending:
        assert _git(clone, "rev-list", "--count", "FETCH_HEAD") == "2"
