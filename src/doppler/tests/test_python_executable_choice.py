"""Which interpreter `make pyext` builds for, and when it refuses (#2364).

`PYTHON_EXECUTABLE` used to be `$(shell uv run python ... 2>/dev/null ||
which python3)`: when uv was installed but REFUSED -- most often because it
is not the version pyproject.toml pins -- the refusal went to /dev/null and
the build quietly took the system python3, so the extension was built for
the wrong interpreter. The fallback is meant only for a box with no uv at
all. These cases pin each branch of the choice through the real Makefile,
with `UV=` pointed at a fake, and never run a build: the refusal is checked
through `python-executable-check`, and its wiring into `pyext` through a dry
run.
"""

from __future__ import annotations

import os
import stat
import subprocess
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
REFUSAL = "error: fake uv refuses (doppler#2364)"


def _fake_uv(tmp_path: Path, body: str) -> Path:
    uv = tmp_path / "uv"
    uv.write_text("#!/bin/sh\n" + body, encoding="utf-8")
    uv.chmod(uv.stat().st_mode | stat.S_IXUSR)
    return uv


def _make(*args: str, env: dict[str, str] | None = None):
    """Run make in the repo; PYTHON_EXECUTABLE/JUST_BUILDIT_PYTHON unset."""
    base = {
        k: v
        for k, v in os.environ.items()
        if k not in ("PYTHON_EXECUTABLE", "JUST_BUILDIT_PYTHON", "MAKEFLAGS")
    }
    return subprocess.run(
        ["make", "-s", "-C", str(REPO), *args],
        capture_output=True,
        text=True,
        env={**base, **(env or {})},
    )


def test_a_refusing_uv_fails_the_check_and_says_why(tmp_path: Path) -> None:
    uv = _fake_uv(tmp_path, f'echo "{REFUSAL}" >&2\nexit 2\n')
    r = _make("python-executable-check", f"UV={uv}")
    out = r.stdout + r.stderr
    assert r.returncode != 0, out
    assert REFUSAL in out, out
    assert "Not falling back to python3" in out, out


def test_pyext_goes_through_the_check(tmp_path: Path) -> None:
    """A dry run prints the check's recipe ahead of the build."""
    uv = _fake_uv(tmp_path, f'echo "{REFUSAL}" >&2\nexit 2\n')
    r = _make("-n", "pyext", f"UV={uv}")
    out = r.stdout + r.stderr
    assert "python-executable-check:" in out, out
    assert "-DPython3_EXECUTABLE=" not in out, out


def test_no_uv_at_all_still_falls_back_to_python3(tmp_path: Path) -> None:
    missing = tmp_path / "no-such-uv"
    check = _make("python-executable-check", f"UV={missing}")
    assert check.returncode == 0, check.stdout + check.stderr
    dry = _make("-n", "pyext", f"UV={missing}")
    assert "-DPython3_EXECUTABLE=" in dry.stdout, dry.stdout + dry.stderr
    assert "python3" in dry.stdout.split("-DPython3_EXECUTABLE=")[1].split()[0]


def test_a_working_uv_supplies_its_venv_interpreter(tmp_path: Path) -> None:
    uv = _fake_uv(tmp_path, 'echo "/fake/venv/bin/python"\n')
    r = _make("-n", "pyext", f"UV={uv}")
    assert "-DPython3_EXECUTABLE=/fake/venv/bin/python" in r.stdout, (
        r.stdout + r.stderr
    )


def test_an_explicit_interpreter_wins_over_a_refusing_uv(
    tmp_path: Path,
) -> None:
    uv = _fake_uv(tmp_path, f'echo "{REFUSAL}" >&2\nexit 2\n')
    r = _make(
        "-n", "pyext", f"UV={uv}", "PYTHON_EXECUTABLE=/opt/py/bin/python3"
    )
    assert "-DPython3_EXECUTABLE=/opt/py/bin/python3" in r.stdout, (
        r.stdout + r.stderr
    )
    jb = _make(
        "python-executable-check",
        f"UV={uv}",
        env={"JUST_BUILDIT_PYTHON": "/opt/jb/python"},
    )
    assert jb.returncode == 0, jb.stdout + jb.stderr
