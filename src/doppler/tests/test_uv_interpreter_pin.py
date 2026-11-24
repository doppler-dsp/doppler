"""The default interpreter is `.python-version`, and make sets no other.

Unset, `uv` picks the newest Python on the box, and a release dry run on a
fresh worktree did exactly that: Python 3.15 has no pillow 12.2.0 wheel, so
`uv sync` built pillow from source and failed. The one declaration is
`.python-version` (3.12); uv reads it for every project command, so every
make target gets it with no Makefile variable.

These cases hold that: the file's content, that the Makefile sets no
UV_PYTHON of its own (a derived or typed default would override the file),
and that uv resolves the file rather than the newest interpreter.
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

ROOT = repo_root(__file__)


def test_the_declaration_is_3_12_and_a_newline() -> None:
    """The one declared default. Changing it is a ruling, not a tidy-up."""
    assert (ROOT / ".python-version").read_bytes() == b"3.12\n"


def _recipe_uv_python() -> str:
    """UV_PYTHON as a make recipe sees it, from an env without one."""
    env = {k: v for k, v in os.environ.items() if k != "UV_PYTHON"}
    r = subprocess.run(
        [
            "make",
            "-s",
            "-C",
            str(ROOT),
            "--eval",
            "probe: ; @printf '%s' \"$$UV_PYTHON\"",
            "probe",
        ],
        env=env,
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stderr
    return r.stdout


def test_make_sets_no_interpreter_of_its_own() -> None:
    """Empty means unset: uv then reads .python-version.

    Sabotage: a `UV_PYTHON ?= ...` line in the Makefile makes this non-empty,
    and the gate (scripts/check_uv_pin.py) names it.
    """
    assert _recipe_uv_python() == ""


@pytest.mark.skipif(shutil.which("uv") is None, reason="uv is not installed")
def test_uv_resolves_the_file_not_the_newest(tmp_path: Path) -> None:
    """A directory with only the file: uv asks for 3.12, not the newest.

    The message names the version it wanted either way, so this holds whether
    or not 3.12 is installed here, and it fails only if uv asks for another.
    """
    shutil.copyfile(ROOT / ".python-version", tmp_path / ".python-version")
    env = {k: v for k, v in os.environ.items() if k != "UV_PYTHON"}
    r = subprocess.run(
        ["uv", "python", "find"],
        cwd=tmp_path,
        env=env,
        capture_output=True,
        text=True,
    )
    said = r.stdout + r.stderr
    assert "3.12" in said, said
    assert "3.15" not in said, said


def test_an_environment_override_still_wins() -> None:
    """A developer's UV_PYTHON chooses the interpreter, as uv documents."""
    env = {**os.environ, "UV_PYTHON": "3.13"}
    r = subprocess.run(
        [
            "make",
            "-s",
            "-C",
            str(ROOT),
            "--eval",
            "probe: ; @printf '%s' \"$$UV_PYTHON\"",
            "probe",
        ],
        env=env,
        capture_output=True,
        text=True,
    )
    assert r.stdout == "3.13"
