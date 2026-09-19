"""The workflow tag-trigger gate, exercised over seeded workflow files.

`scripts/check_workflow_tag_triggers.py` exists because GitHub does not
evaluate a push trigger's `paths:` for TAG pushes: ci-image.yml, filtered to
its Dockerfile and bootstrap.toml, rebuilt and published the CI image on the
v0.51.0 release tag anyway -- 19.4 minutes for a tree that could not have
changed it. Each case seeds a `.github/workflows/` and points `--root` at it.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_workflow_tag_triggers.py"


def _run(tmp_path: Path, on_block: str) -> subprocess.CompletedProcess[str]:
    wf = tmp_path / ".github" / "workflows"
    wf.mkdir(parents=True)
    (wf / "w.yml").write_text(
        f"name: w\non:\n{on_block}jobs:\n  a:\n    runs-on: ubuntu-latest\n"
        "    steps:\n      - run: true\n",
        encoding="utf-8",
    )
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
        capture_output=True,
        text=True,
    )


def test_paths_alone_fires_on_tags_and_is_refused(tmp_path: Path) -> None:
    """The ci-image.yml shape as it shipped."""
    r = _run(tmp_path, "  push:\n    paths:\n      - Dockerfile\n")
    assert r.returncode == 1, r.stdout
    assert ".github/workflows/w.yml" in r.stdout


def test_a_bare_push_is_refused(tmp_path: Path) -> None:
    r = _run(tmp_path, "  push:\n")
    assert r.returncode == 1, r.stdout


@pytest.mark.parametrize(
    "block",
    [
        "  push:\n    branches: ['**']\n    paths:\n      - Dockerfile\n",
        "  push:\n    branches: [main]\n",
        "  push:\n    tags:\n      - 'v[0-9]*'\n",
        "  push:\n    tags-ignore: ['**']\n",
        "  pull_request:\n",
        "  workflow_dispatch:\n",
    ],
    ids=["branches-glob", "branches", "tags", "tags-ignore", "pr", "manual"],
)
def test_an_explicit_choice_passes(tmp_path: Path, block: str) -> None:
    r = _run(tmp_path, block)
    assert r.returncode == 0, r.stdout


def test_no_workflows_is_not_a_pass(tmp_path: Path) -> None:
    (tmp_path / ".github" / "workflows").mkdir(parents=True)
    r = subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 1, r.stdout
