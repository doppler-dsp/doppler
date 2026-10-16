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

import stat
import subprocess
from pathlib import PurePath
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import nested_make_env, repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
REFUSAL = "error: fake uv refuses (doppler#2364)"


def _fake_uv(tmp_path: Path, body: str) -> str:
    """A fake uv, as the POSIX path a recipe can run.

    make hands `UV=` to its shell, where a Windows path's backslashes are
    eaten as escapes (test_uv_pin_gate.py's lock gate has the same note;
    ci.yml runs this suite on windows-latest)."""
    uv = tmp_path / "uv"
    uv.write_text("#!/bin/sh\n" + body, encoding="utf-8")
    uv.chmod(uv.stat().st_mode | stat.S_IXUSR)
    return PurePath(uv).as_posix()


def _make(*args: str, env: dict[str, str] | None = None):
    """Run make in the repo; PYTHON_EXECUTABLE/JUST_BUILDIT_PYTHON unset."""
    base = nested_make_env("PYTHON_EXECUTABLE", "JUST_BUILDIT_PYTHON")
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


@pytest.mark.parametrize(
    "target",
    ["pyext", "coverage", "warnings-check", "test-example-downstream-python"],
)
def test_each_python_target_goes_through_the_check(
    tmp_path: Path, target: str
) -> None:
    """A dry run prints the check's recipe ahead of the build."""
    uv = _fake_uv(tmp_path, f'echo "{REFUSAL}" >&2\nexit 2\n')
    r = _make("-n", target, f"UV={uv}")
    out = r.stdout + r.stderr
    assert "python-executable-check:" in out, out
    assert "-DPython3_EXECUTABLE=" not in out, out


def test_the_reason_is_uvs_error_not_a_warning_before_it(
    tmp_path: Path,
) -> None:
    uv = _fake_uv(
        tmp_path,
        'echo "warning: a notice uv prints first" >&2\n'
        f'echo "{REFUSAL}" >&2\nexit 2\n',
    )
    r = _make("python-executable-check", f"UV={uv}")
    out = r.stdout + r.stderr
    assert r.returncode != 0, out
    assert REFUSAL in out, out
    assert "a notice uv prints first" not in out, out


def test_an_empty_explicit_interpreter_is_refused(tmp_path: Path) -> None:
    """`PYTHON_EXECUTABLE=` given empty would drop the CMake flag."""
    uv = _fake_uv(tmp_path, 'echo "/fake/venv/bin/python"\n')
    r = _make("python-executable-check", f"UV={uv}", "PYTHON_EXECUTABLE=")
    out = r.stdout + r.stderr
    assert r.returncode != 0, out
    assert "PYTHON_EXECUTABLE is empty" in out, out


def test_no_uv_at_all_still_falls_back_to_python3(tmp_path: Path) -> None:
    missing = PurePath(tmp_path / "no-such-uv").as_posix()
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
