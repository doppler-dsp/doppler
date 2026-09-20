"""The tracked-paths gate, exercised over a seeded git repository.

``make tracked-paths-check`` -- vendored from standard.mk, not owned here --
enforces two properties of the set of tracked paths, neither of which any
build, formatter or docs gate would ever open a file to find:

- every name is one a person could type (``[A-Za-z0-9._/-]``), because a
  path carrying a shell's punctuation is the residue of a quoting slip;
- no two names differ only in case, because Windows and macOS keep only one
  of them and the checkout shows the other as a phantom edit. The one pair
  this repo carried was a jm scaffold (``test_Resampler.py``) beside the real
  suite (``test_resampler.py``), found by a Windows checkout.

The rule moved upstream so every repo gets it from one place; the exercise
stayed here, because this is where the test harness is. Each case builds its
own repository -- a gate that can only run against the real tree cannot be
sabotaged, and a gate never seen to fail is decoration.
"""

from __future__ import annotations

import subprocess
from typing import TYPE_CHECKING

from doppler.tests._platform import posix_only
from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
STANDARD_MK = REPO / "standard.mk"

#: Enough configuration for standard.mk to load. STANDARD_URL is emptied so
#: the throwaway repo does not try to check its own vendored copy for drift.
_MAKEFILE = """\
STANDARD_URL =
TEST_CMD = @true
TEST_FAST_CMD = @true
SYNC_CMD = @true
CLEAN_CMD = @true
FORMAT_TOOLS =
include standard.mk
"""


def _repo(tmp_path: Path, names: list[str]) -> Path:
    """A git repository tracking exactly *names*, able to run the gate."""
    subprocess.run(["git", "init", "-q", str(tmp_path)], check=True)
    for name in names:
        p = tmp_path / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(name + "\n", encoding="utf-8")
    (tmp_path / "standard.mk").write_text(
        STANDARD_MK.read_text(encoding="utf-8"), encoding="utf-8"
    )
    (tmp_path / "Makefile").write_text(_MAKEFILE, encoding="utf-8")
    subprocess.run(
        ["git", "-C", str(tmp_path), "add", "--", *names], check=True
    )
    return tmp_path


def _run(root: Path) -> subprocess.CompletedProcess[str]:
    """Run the gate. make reports a failed recipe as 2, not the rule's 1."""
    return subprocess.run(
        ["make", "-s", "tracked-paths-check"],
        cwd=root,
        capture_output=True,
        text=True,
    )


def test_ordinary_names_pass(tmp_path: Path) -> None:
    r = _run(_repo(tmp_path, ["src/a.py", "src/B.py", "docs/x-y_z.md"]))
    assert r.returncode == 0, r.stdout + r.stderr


@posix_only("seeds a file name NTFS forbids (it holds '>')")
def test_a_shell_fragment_name_fails(tmp_path: Path) -> None:
    r = _run(_repo(tmp_path, ["src/a.py", "x (self->h);,+25p"]))
    assert r.returncode != 0, r.stdout + r.stderr
    assert "outside [A-Za-z0-9._/-]" in r.stderr


@posix_only(
    "seeds two names differing only in case, which NTFS folds into one"
)
def test_names_differing_only_in_case_fail(tmp_path: Path) -> None:
    names = ["tests/test_Resampler.py", "tests/test_resampler.py"]
    r = _run(_repo(tmp_path, [*names, "tests/test_cic.py"]))
    assert r.returncode != 0, r.stdout + r.stderr
    assert "differ only in case" in r.stderr
    for n in names:  # both members of the group are named
        assert n in r.stderr
    assert "test_cic.py" not in r.stderr


@posix_only(
    "seeds two names differing only in case, which NTFS folds into one"
)
def test_a_directory_case_collision_fails(tmp_path: Path) -> None:
    """The same file under two spellings of one DIRECTORY collides too."""
    r = _run(_repo(tmp_path, ["Docs/a.md", "docs/a.md"]))
    assert r.returncode != 0, r.stdout + r.stderr


def test_the_hook_hands_the_gate_no_filenames() -> None:
    """A collision is a relation between two names, so a run given only the
    staged one cannot see it. The gate reads the whole tree and takes no
    arguments; this pins the hook that calls it to match."""
    cfg = (REPO / ".pre-commit-config.yaml").read_text(encoding="utf-8")
    block = cfg.split("- id: tracked-paths", 1)[1].split("- id:", 1)[0]
    assert "pass_filenames: false" in block
    assert "always_run: true" in block
