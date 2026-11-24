"""A parse-only make never changes the shared .venv.

Why this exists (#3305): the Makefile asked "which Python" with a plain
`uv run python`. With UV_PYTHON unset, uv reads .python-version (3.12) and
REBUILDS a 3.14 .venv as 3.12 ("Removed virtual environment at: .venv"). A
test that ran make with a minimal environment did that mid-suite, and the
rest of the run lost its packages (`No module named 'pygments'`, then
'matplotlib'). The probe now asks without syncing: `run --no-sync` against an
existing venv, `uv python find` when there is none.

The case: a throwaway project with a real 3.14 venv, make -n on a target with
UV_PYTHON unset. Sabotage: restore the plain `uv run python` probe and the
venv's pyvenv.cfg changes; the test goes red.
"""

from __future__ import annotations

import os
import shutil
import subprocess
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
PROJECT_FILES = [
    "Makefile",
    "standard.mk",
    "pyproject.toml",
    ".python-version",
]


def _python314() -> str | None:
    """A real 3.14 interpreter path from uv, or None (the case is skipped)."""
    if shutil.which("uv") is None:
        return None
    env = {k: v for k, v in os.environ.items() if k != "VIRTUAL_ENV"}
    r = subprocess.run(
        ["uv", "python", "find", "3.14"],
        env=env,
        capture_output=True,
        text=True,
    )
    return r.stdout.strip() if r.returncode == 0 else None


@pytest.mark.skipif(_python314() is None, reason="no 3.14 interpreter here")
def test_a_parse_only_make_leaves_a_3_14_venv_alone(tmp_path: Path) -> None:
    project = tmp_path / "proj"
    project.mkdir()
    for name in PROJECT_FILES:
        shutil.copy(REPO / name, project / name)
    shutil.copytree(REPO / "scripts", project / "scripts")
    subprocess.run(
        ["uv", "venv", "--quiet", "--python", _python314(), ".venv"],
        cwd=project,
        check=True,
    )
    cfg = project / ".venv" / "pyvenv.cfg"
    before = (cfg.read_bytes(), cfg.stat().st_mtime_ns)
    env = {k: v for k, v in os.environ.items() if k != "UV_PYTHON"}
    r = subprocess.run(
        ["make", "-n", "-C", str(project), "test-python"],
        env=env,
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stdout + r.stderr
    after = (cfg.read_bytes(), cfg.stat().st_mtime_ns)
    assert after == before, (
        "a parse-only make rewrote the venv (pyvenv.cfg changed):\n"
        f"{before[0].decode()!r}\n->\n{after[0].decode()!r}"
    )
