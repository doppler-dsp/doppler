"""The default interpreter is `.python-version`, and make sets no other.

Unset, `uv` picks the newest Python on the box, and a release dry run on a
fresh worktree did exactly that: Python 3.15 has no pillow 12.2.0 wheel, so
`uv sync` built pillow from source and failed. The one declaration is
`.python-version` (3.12); uv reads it for every project command, so every
make target gets it with no Makefile variable.

These cases read the files and ask uv about an empty directory. They do NOT
run make or any project uv command: a make invocation runs the Makefile's
parse-time `uv run`, which syncs the shared `.venv`, and a test that does that
mid-run replaces the interpreter under the rest of the suite (seen on the
3.14 leg of #3305 as `No module named 'matplotlib'`).
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

ROOT = repo_root(__file__)

#: The one UV_PYTHON assignment the Makefile may make: the smoke override.
_ASSIGN = re.compile(r"^[^#\n]*\bUV_PYTHON\s*[?:+!]?=", re.MULTILINE)


def test_the_declaration_is_3_12_and_a_newline() -> None:
    """The one declared default. Changing it is a ruling, not a tidy-up."""
    assert (ROOT / ".python-version").read_bytes() == b"3.12\n"


def test_the_makefile_sets_no_interpreter_of_its_own() -> None:
    """A derived or typed default would override the file.

    The only UV_PYTHON assignment is the smoke target's explicit override.
    Sabotage: a `UV_PYTHON ?= ...` line makes a second match here, and the
    gate (scripts/check_uv_pin.py) names it too.
    """
    text = (ROOT / "Makefile").read_text(encoding="utf-8")
    hits = [m.group(0).strip() for m in _ASSIGN.finditer(text)]
    assert hits == [
        "release-smoke-pypi: export UV_PYTHON = $(SMOKE_PYTHON)"
    ] or (len(hits) == 1 and "release-smoke-pypi" in hits[0]), hits


@pytest.mark.skipif(shutil.which("uv") is None, reason="uv is not installed")
def test_uv_resolves_the_file_not_the_newest(tmp_path: Path) -> None:
    """A bare directory with only the file: uv asks for 3.12, not the newest.

    `uv python find` names what it wanted, installed or not, and creates no
    project environment, so this runs beside the suite safely.
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


# An environment UV_PYTHON is NOT tested here: `uv python find` ignores it when
# a .python-version is present (measured, uv 0.13.6), so asserting it would
# test the wrong command. The override is `uv sync`'s, and it is measured
# there: `UV_PYTHON=3.13 uv sync` says "Using CPython 3.13.18", and CI's legs
# say "Using CPython 3.14.10" under setup-uv's UV_PYTHON (#3305 run logs).
