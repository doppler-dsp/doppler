"""A scratch-repo test must never write into the caller's repository.

``git rebase -x CMD`` exports ``GIT_DIR`` (and ``GIT_PREFIX``) to ``CMD``,
and a git hook may export the other redirect variables. Many gate tests
build a scratch repository (``git init <tmp>``, ``git -C <tmp> add .``);
with ``GIT_DIR`` inherited, those calls re-initialise and rewrite the
CALLER's index instead of the scratch one. A per-commit replay of a pull
request ended that way with 3,771 staged deletions and 38 gate tests red
(#2212).

The root ``conftest.py`` clears the redirect variables at import, in every
pytest process. This module proves it end to end: a nested pytest run,
started with ``GIT_DIR`` pointing at a decoy repository, runs one real
scratch-repo gate test, and the decoy's index must come out byte for byte
as it went in.
"""

from __future__ import annotations

import os
import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

#: The checkout, found by its markers: `make coverage` runs this file from a
#: copy under build-cov/, where a fixed number of parents is not the repo.
ROOT = repo_root(__file__)

#: One real gate test that builds a scratch repository with ``git init``
#: and ``git -C <tmp> add .``: the shape #2212 was hit through.
_SCRATCH_TEST = (
    "src/doppler/tests/test_named_args_gate.py"
    "::test_named_arguments_pass_in_every_shape"
)


def _git(cwd: Path, *args: str) -> None:
    subprocess.run(["git", *args], cwd=cwd, check=True, capture_output=True)


@pytest.mark.parametrize("workers", [[], ["-n", "2"]], ids=["serial", "xdist"])
def test_an_inherited_git_dir_leaves_the_callers_index_alone(
    tmp_path: Path, workers: list[str]
) -> None:
    """Run under ``GIT_DIR=<decoy>/.git``, a scratch-repo gate test passes
    and the decoy's index is unchanged, in one process and in an xdist
    worker.

    The decoy stands for the caller's repository: one staged file, so its
    index exists and has content to lose. The ``xdist`` case runs the gate
    test in a worker, as ``make test-python`` (``-n auto``) does: a worker
    inherits the controller's environment AND loads ``conftest.py`` itself,
    so it must come out clean either way. The nested run gets its own
    ``--basetemp`` (a nested pytest using the default would prune the outer
    run's numbered temp directories) and no cache provider (it would write
    ``.pytest_cache`` into the repo root). Without the conftest's clearing,
    the gate test's ``git -C <tmp> add .`` stages the scratch tree into the
    decoy's index, and the comparison fails.
    """
    decoy = tmp_path / "decoy"
    decoy.mkdir()
    _git(tmp_path, "init", "-q", str(decoy))
    (decoy / "kept.txt").write_text("the caller's work\n", encoding="utf-8")
    _git(decoy, "add", "kept.txt")
    index = decoy / ".git" / "index"
    before = index.read_bytes()

    env = {**os.environ, "GIT_DIR": str(decoy / ".git"), "GIT_PREFIX": ""}
    r = subprocess.run(
        [
            sys.executable,
            "-m",
            "pytest",
            "-q",
            "-p",
            "no:cacheprovider",
            f"--basetemp={tmp_path / 'nested'}",
            *workers,
            _SCRATCH_TEST,
        ],
        cwd=ROOT,
        env=env,
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stdout + r.stderr
    assert index.read_bytes() == before, "the decoy's index was rewritten"
