"""scripts/uvg.py: a nested make refuses to rebuild a mismatched .venv.

The guard is what `DEV_RUN` runs through. A nested make (MAKELEVEL > 0) whose
request names a different Python than the existing .venv is refused with one
line naming both versions; a top-level make, or a request that matches, runs
the command. Sabotage: drop the guard from the script (exec "$@" only) and the
refusal case goes green-to-red.
"""

from __future__ import annotations

import os
import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

GUARD = repo_root(__file__) / "scripts" / "uvg.py"


def _project(tmp_path: Path, venv_version: str, pin: str) -> Path:
    (tmp_path / ".venv").mkdir()
    (tmp_path / ".venv" / "pyvenv.cfg").write_text(
        f"version_info = {venv_version}\n", encoding="utf-8"
    )
    (tmp_path / ".python-version").write_text(f"{pin}\n", encoding="utf-8")
    return tmp_path


def _run(
    cwd: Path, *, makelevel: str | None, uv_python: str | None = None
) -> subprocess.CompletedProcess[str]:
    env = {
        k: v
        for k, v in os.environ.items()
        if k not in ("MAKELEVEL", "UV_PYTHON")
    }
    if makelevel is not None:
        env["MAKELEVEL"] = makelevel
    if uv_python is not None:
        env["UV_PYTHON"] = uv_python
    return subprocess.run(
        [sys.executable, str(GUARD), "echo", "run", "ran"],
        cwd=cwd,
        env=env,
        capture_output=True,
        text=True,
    )


def test_a_nested_make_refuses_a_mismatched_venv(tmp_path: Path) -> None:
    r = _run(_project(tmp_path, "3.14.4", "3.12"), makelevel="1")
    assert r.returncode == 2, r.stdout + r.stderr
    assert "the .venv is on Python 3.14; this request is 3.12" in r.stderr
    assert "Run the same make" in r.stderr
    assert "run ran" not in r.stdout


def test_a_top_level_make_is_not_refused(tmp_path: Path) -> None:
    """The top-level make may build the venv it was asked for."""
    r = _run(_project(tmp_path, "3.14.4", "3.12"), makelevel="0")
    assert r.returncode == 0, r.stderr
    assert r.stdout.strip() == "run ran"


def test_a_matching_venv_is_not_refused(tmp_path: Path) -> None:
    r = _run(_project(tmp_path, "3.12.16", "3.12"), makelevel="1")
    assert r.returncode == 0, r.stderr


def test_the_environment_request_wins_over_the_file(tmp_path: Path) -> None:
    """UV_PYTHON, as uv reads it, is the request when it is set."""
    r = _run(
        _project(tmp_path, "3.14.4", "3.12"), makelevel="1", uv_python="3.14"
    )
    assert r.returncode == 0, r.stderr
