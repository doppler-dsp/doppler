"""The Python-versions gate and the CI matrix it feeds, over seeded pyprojects.

`scripts/python_versions.py` is the one reader of the supported Python set:
CI's `classify` step derives the matrix from the classifiers through it, and
`make lint` holds the classifiers to `requires-python`. Each drift case seeds
a pyproject that would make the matrix test a set the package does not
declare, so it fails if the gate cannot see it.
"""

from __future__ import annotations

import json
import subprocess
import sys
import textwrap
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "python_versions.py"


def _run(tmp_path: Path, requires: str, versions: list[str], *args: str):
    classifiers = "\n".join(
        f'    "Programming Language :: Python :: {v}",' for v in versions
    )
    f = tmp_path / "pyproject.toml"
    f.write_text(
        textwrap.dedent(
            f"""\
            [project]
            name = "x"
            requires-python = "{requires}"
            classifiers = [
                "Programming Language :: Python :: 3",
            {classifiers}
            ]
            """
        ),
        encoding="utf-8",
    )
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--pyproject", str(f), *args],
        capture_output=True,
        text=True,
    )


def test_the_repos_own_pyproject_passes() -> None:
    r = subprocess.run(
        [sys.executable, str(SCRIPT)], capture_output=True, text=True
    )
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_matching_set_passes(tmp_path: Path) -> None:
    r = _run(tmp_path, ">=3.9", ["3.9", "3.10"])
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_raised_floor_with_the_classifiers_unchanged_fails(
    tmp_path: Path,
) -> None:
    r = _run(tmp_path, ">=3.10", ["3.9", "3.10"])
    assert r.returncode == 1
    assert "not the requires-python floor 3.10" in r.stderr


def test_a_classifier_below_the_floor_fails(tmp_path: Path) -> None:
    r = _run(tmp_path, ">=3.9", ["3.8", "3.9", "3.10"])
    assert r.returncode == 1
    assert "classifier 3.8 is outside" in r.stderr


def test_a_classifier_above_an_upper_bound_fails(tmp_path: Path) -> None:
    r = _run(tmp_path, ">=3.9,<3.11", ["3.9", "3.10", "3.11"])
    assert r.returncode == 1
    assert "classifier 3.11 is outside" in r.stderr


def test_the_matrix_sorts_by_version_not_as_text(
    tmp_path: Path,
) -> None:
    # "3.10" < "3.9" as strings; the matrix must not put 3.10 first.
    r = _run(tmp_path, ">=3.9", ["3.10", "3.9", "3.14"], "--matrix")
    assert r.returncode == 0, r.stderr
    assert json.loads(r.stdout) == ["3.9", "3.10", "3.14"]


def test_the_primary_leg_is_the_floor(tmp_path: Path) -> None:
    # Bare, not JSON: ci.yml compares it to matrix.python-version as a string.
    r = _run(tmp_path, ">=3.10", ["3.12", "3.10", "3.11"], "--primary")
    assert r.returncode == 0, r.stderr
    assert r.stdout == "3.10\n"


def test_the_primary_follows_the_classifiers(tmp_path: Path) -> None:
    """A floor raised past the old literal moves the primary with it."""
    r = _run(tmp_path, ">=3.13", ["3.13", "3.14"], "--primary")
    assert r.returncode == 0, r.stderr
    assert r.stdout == "3.13\n"
