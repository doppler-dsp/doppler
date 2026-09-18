"""The tracked-paths gate, exercised over a seeded git repository.

`scripts/check-tracked-paths.sh` enforces two properties of the set of
tracked paths, both of which no build, formatter or docs gate would ever
open a file to find:

- every name is one a person could type (``[A-Za-z0-9._/-]``), because a
  path carrying a shell's punctuation is the residue of a quoting slip;
- no two names differ only in case, because Windows and macOS keep only one
  of them and the checkout shows the other as a phantom edit. The one pair
  the repo carried was a jm scaffold (``test_Resampler.py``) beside the real
  suite (``test_resampler.py``), found by a Windows checkout.

Each case builds its own repository, because a gate that can only run
against the real tree cannot be sabotaged.
"""

from __future__ import annotations

import subprocess
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check-tracked-paths.sh"


def _repo(tmp_path: Path, names: list[str]) -> Path:
    """A git repository tracking exactly *names*."""
    subprocess.run(["git", "init", "-q", str(tmp_path)], check=True)
    for name in names:
        p = tmp_path / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(name + "\n", encoding="utf-8")
    subprocess.run(
        ["git", "-C", str(tmp_path), "add", "--", *names], check=True
    )
    return tmp_path


def _run(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["sh", str(SCRIPT)], cwd=root, capture_output=True, text=True
    )


def test_ordinary_names_pass(tmp_path: Path) -> None:
    r = _run(_repo(tmp_path, ["src/a.py", "src/B.py", "docs/x-y_z.md"]))
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_shell_fragment_name_fails(tmp_path: Path) -> None:
    r = _run(_repo(tmp_path, ["src/a.py", "x (self->h);,+25p"]))
    assert r.returncode == 1, r.stdout + r.stderr
    assert "outside [A-Za-z0-9._/-]" in r.stderr


def test_names_differing_only_in_case_fail(tmp_path: Path) -> None:
    names = ["tests/test_Resampler.py", "tests/test_resampler.py"]
    r = _run(_repo(tmp_path, [*names, "tests/test_cic.py"]))
    assert r.returncode == 1, r.stdout + r.stderr
    assert "differ only in case" in r.stderr
    for n in names:  # both members of the group are named
        assert n in r.stderr
    assert "test_cic.py" not in r.stderr


def test_a_directory_case_collision_fails(tmp_path: Path) -> None:
    """The same file under two spellings of one DIRECTORY collides too."""
    r = _run(_repo(tmp_path, ["Docs/a.md", "docs/a.md"]))
    assert r.returncode == 1, r.stdout + r.stderr


def test_the_collision_is_seen_when_handed_one_path(tmp_path: Path) -> None:
    """pre-commit hands the script only the staged names; a collision is a
    relation between two, so it must still be found against the tree."""
    root = _repo(tmp_path, ["t/test_A.py", "t/test_a.py"])
    r = subprocess.run(
        ["sh", str(SCRIPT), "t/test_A.py"],
        cwd=root,
        capture_output=True,
        text=True,
    )
    assert r.returncode == 1, r.stdout + r.stderr
