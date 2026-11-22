"""The release workflow's runners are pinned, and the gate says so.

`scripts/check_release_runners.py` exists because of #3188: v0.84.0's
x86_64 PyPI smoke ran on `ubuntu-latest`, which had rolled to 26.04, where
`actions/setup-python` has no 3.9 -- after the wheels were published. The
cases seed a workflow, so the gate is tested without breaking the real one,
and each asserts it goes RED.
"""

from __future__ import annotations

import subprocess
import sys
import textwrap
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_release_runners.py"


def _run(*files: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *map(str, files)],
        capture_output=True,
        text=True,
        check=False,
    )


def _wf(tmp: Path, body: str) -> Path:
    f = tmp / "release.yml"
    f.write_text(textwrap.dedent(body), encoding="utf-8")
    return f


def test_a_pinned_workflow_passes(tmp_path: Path) -> None:
    r = _run(
        _wf(
            tmp_path,
            """
            jobs:
              a:
                runs-on: ubuntu-24.04
              b:
                strategy:
                  matrix:
                    arch:
                      - { tag: x86_64, runner: ubuntu-24.04 }
                      - { tag: win, runner: windows-2025-vs2026 }
                runs-on: ${{ matrix.arch.runner }}
            # ubuntu-latest in a comment is prose, not a runner
            """,
        )
    )
    assert r.returncode == 0, r.stdout


def test_a_plain_runs_on_latest_is_refused(tmp_path: Path) -> None:
    r = _run(_wf(tmp_path, "jobs:\n  a:\n    runs-on: ubuntu-latest\n"))
    assert r.returncode == 1
    assert "ubuntu-latest" in r.stdout and "/jobs/a/runs-on" in r.stdout
    assert "early warning" in r.stdout  # the ci.yml asymmetry is stated


def test_a_matrix_entry_latest_is_refused(tmp_path: Path) -> None:
    r = _run(
        _wf(
            tmp_path,
            """
            jobs:
              s:
                strategy:
                  matrix:
                    arch:
                      - { tag: x86_64, runner: ubuntu-latest }
                      - { tag: win, runner: windows-latest }
                runs-on: ${{ matrix.arch.runner }}
            """,
        )
    )
    assert r.returncode == 1
    assert "ubuntu-latest" in r.stdout and "windows-latest" in r.stdout


def test_an_expression_carrying_latest_is_refused(tmp_path: Path) -> None:
    r = _run(
        _wf(
            tmp_path,
            "jobs:\n  a:\n    runs-on: ${{ inputs.x || 'macos-latest' }}\n",
        )
    )
    assert r.returncode == 1 and "macos-latest" in r.stdout


def test_the_real_release_workflow_passes() -> None:
    r = _run()
    assert r.returncode == 0, r.stdout
