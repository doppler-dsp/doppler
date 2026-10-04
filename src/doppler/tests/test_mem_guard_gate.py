"""The memory-guard gate, exercised over seeded makefiles.

`scripts/check_mem_guarded.py` guards against the WSL VM being killed
outright -- not a test failing, the machine dying with every shell and SSH
session on it. It happened on 2026-10-01 and again on 2026-10-03, both times
the docs build (zensical, 6.0 GiB resident) beside an xdist pytest run, each
harmless alone. `scripts/mem-guard.sh` now holds every guarded command to
one shared slice ceiling; this gate is what keeps the heavy commands inside
it.

A gate proven only against a tree that happens to pass is a gate nobody has
seen fail. So each test here seeds a makefile in the shape of a real call
site and requires the gate to go red without the guard and green with it,
plus the two false-positive traps hit while writing it.
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
SCRIPT = REPO / "scripts" / "check_mem_guarded.py"

GUARD = "MEM_GUARD_CMD = scripts/mem-guard.sh\n"

# TEST_PYTHON_CMD's shape: an assignment continued over two physical lines,
# with the xdist flag on the second.
PYTEST_ASSIGN = (
    GUARD
    + "TEST_PYTHON_CMD = {guard}uv run pytest src/ -v \\\n"
    + "                      --benchmark-disable -n auto\n"
    + "test-python:\n\t$(TEST_PYTHON_CMD)\n"
)

# test-examples-python's shape: the parallel pytest in the recipe itself.
PYTEST_RECIPE = (
    GUARD + "test-examples:\n\t{guard}uv run pytest -m examples \\\n"
    "\t    -q -n auto src/doppler/tests/test_examples.py\n"
)

# The docs shape: zensical defined once, invoked only through variables
# (standard.mk's DOCS_BUILD_CMD), with a vendored default the repo overrides.
DOCS = (
    GUARD + "ZENSICAL = {guard}uv run --group docs zensical\n"
    "ZENSICAL ?= $(DEV_RUN) zensical\n"
    "DOCS_BUILD_CMD ?= $(ZENSICAL) build --clean --strict\n"
    "docs:\n\t$(DOCS_BUILD_CMD)\n"
)


def _check(tmp_path: Path, body: str) -> subprocess.CompletedProcess[str]:
    mk = tmp_path / "Seeded.mk"
    mk.write_text(body, encoding="utf-8")
    return subprocess.run(
        [sys.executable, str(SCRIPT), str(mk)],
        capture_output=True,
        text=True,
        cwd=REPO,
    )


SHAPES = [PYTEST_ASSIGN, PYTEST_RECIPE, DOCS]
IDS = ["pytest-assignment", "pytest-recipe", "zensical"]


@pytest.mark.parametrize("shape", SHAPES, ids=IDS)
def test_an_unguarded_heavy_command_is_caught(
    tmp_path: Path, shape: str
) -> None:
    """Each real call-site shape, as it stood before the fix, goes red."""
    proc = _check(tmp_path, shape.format(guard=""))
    assert proc.returncode == 1, proc.stdout
    assert "outside the memory guard" in proc.stdout


@pytest.mark.parametrize("shape", SHAPES, ids=IDS)
def test_the_guard_through_its_variable_clears_it(
    tmp_path: Path, shape: str
) -> None:
    """`$(MEM_GUARD_CMD)` counts once expanded, wherever it is reached."""
    proc = _check(tmp_path, shape.format(guard="$(MEM_GUARD_CMD) "))
    assert proc.returncode == 0, proc.stdout


def test_a_shell_n_test_is_not_xdist(tmp_path: Path) -> None:
    """`[ -n "$$x" ]` in a recipe that also runs a serial pytest.

    The first draft read any `-n` as xdist and flagged two serial recipes,
    which a gate that cries wolf teaches people to prefix blindly.
    """
    body = (
        GUARD + 'docs-snippets:\n\t@x=1; [ -n "$$x" ] && '
        "uv run pytest -q src/doppler/tests\n"
        "test-parallel:\n\t$(MEM_GUARD_CMD) uv run pytest -n auto\n"
    )
    proc = _check(tmp_path, body)
    assert proc.returncode == 0, proc.stdout


def test_n_zero_is_serial(tmp_path: Path) -> None:
    """`-n 0` is xdist switched off -- the documented PYTEST_ARGS override."""
    body = (
        GUARD + "serial:\n\tuv run pytest -n 0 src/\n"
        "test-parallel:\n\t$(MEM_GUARD_CMD) uv run pytest -n auto\n"
    )
    proc = _check(tmp_path, body)
    assert proc.returncode == 0, proc.stdout


def test_no_heavy_command_at_all_is_not_a_pass(tmp_path: Path) -> None:
    """Zero matches means the parser stopped matching, not a clean tree."""
    proc = _check(tmp_path, GUARD + "build:\n\tcmake --build build\n")
    assert proc.returncode == 1, proc.stdout
    assert "stopped matching" in proc.stdout


def test_the_repository_itself_passes() -> None:
    """The real Makefile and standard.mk, read as `make lint` reads them."""
    proc = subprocess.run(
        [sys.executable, str(SCRIPT)],
        capture_output=True,
        text=True,
        cwd=REPO,
    )
    assert proc.returncode == 0, proc.stdout
