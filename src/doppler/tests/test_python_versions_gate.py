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


def _release(built: list[str]) -> str:
    """A release.yml with the three build-list shapes release.yml has: one
    in cp tags, one in quoted versions, and the derived smoke matrix (an
    expression, which the gate must skip)."""
    cp = ", ".join("cp" + v.replace(".", "") for v in built)
    dotted = ", ".join(f'"{v}"' for v in built)
    return textwrap.dedent(
        f"""\
        jobs:
          build-python:
            strategy:
              matrix:
                python: [{cp}]
          build-macos:
            strategy:
              matrix:
                python: [{dotted}]
          smoke-pypi:
            strategy:
              matrix:
                python: ${{{{ fromJSON(needs.v.outputs.pythons) }}}}
        """
    )


def _run(
    tmp_path: Path,
    requires: str,
    versions: list[str],
    *args: str,
    release: str | None = None,
):
    """Run the script on a seeded pyproject and release.yml. By default the
    release builds exactly the classifier set, so a case about the pyproject
    is not also a case about the release."""
    rel = tmp_path / "release.yml"
    rel.write_text(
        release
        if release is not None
        else _release(
            sorted(versions, key=lambda v: tuple(map(int, v.split("."))))
        ),
        encoding="utf-8",
    )
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
        [
            sys.executable,
            str(SCRIPT),
            "--pyproject",
            str(f),
            "--release",
            str(rel),
            *args,
        ],
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


# ── release.yml's build lists, both directions (doppler#1964) ───────────────


def test_a_classifier_with_no_build_entry_fails(tmp_path: Path) -> None:
    """3.11 declared and smoked, but no job builds its wheel."""
    r = _run(
        tmp_path,
        ">=3.9",
        ["3.9", "3.10", "3.11"],
        release=_release(["3.9", "3.10"]),
    )
    assert r.returncode == 1
    assert "builds no cp311 wheel" in r.stderr
    assert r.stderr.count("builds no cp311") == 2  # both literal lists


def test_a_build_entry_with_no_classifier_fails(tmp_path: Path) -> None:
    """A cp311 wheel built and shipped that no smoke leg installs."""
    r = _run(
        tmp_path,
        ">=3.9",
        ["3.9", "3.10"],
        release=_release(["3.9", "3.10", "3.11"]),
    )
    assert r.returncode == 1
    assert "builds cp311, which no classifier declares" in r.stderr


def test_a_release_with_no_literal_build_list_fails(tmp_path: Path) -> None:
    """Nothing to compare is not a pass: a renamed key would hide the gate."""
    r = _run(
        tmp_path, ">=3.9", ["3.9"], release="jobs:\n  x:\n    runs-on: y\n"
    )
    assert r.returncode == 1
    assert "found no job whose matrix python is a list" in r.stderr


#: build-windows's list as a YAML BLOCK list: the spelling a text scan for
#: `python: [...]` skipped, so the gate passed with that job unchecked.
_BLOCK = """\
  build-windows:
    strategy:
      matrix:
        python:
          - "3.9"
          - "3.10"
"""


def test_a_block_list_is_checked_like_a_flow_list(tmp_path: Path) -> None:
    ok = _run(
        tmp_path,
        ">=3.9",
        ["3.9", "3.10"],
        release=_release(["3.9", "3.10"]) + _BLOCK,
    )
    assert ok.returncode == 0, ok.stderr
    assert "build-windows" in ok.stdout


def test_a_block_list_missing_a_classifier_fails(tmp_path: Path) -> None:
    r = _run(
        tmp_path,
        ">=3.9",
        ["3.9", "3.10", "3.11"],
        release=_release(["3.9", "3.10", "3.11"]) + _BLOCK,
    )
    assert r.returncode == 1
    assert "job build-windows: builds no cp311 wheel" in r.stderr


def test_an_unquoted_version_is_not_read_as_another(tmp_path: Path) -> None:
    """YAML reads an unquoted 3.10 as the float 3.1; flag it, never treat
    it as some version."""
    rel = (
        "jobs:\n  b:\n    strategy:\n      matrix:\n"
        "        python: [3.9, 3.10]\n"
    )
    r = _run(tmp_path, ">=3.9", ["3.9", "3.10"], release=rel)
    assert r.returncode == 1
    assert "neither cp3N nor quoted 3.N" in r.stderr


def test_a_python_that_is_neither_list_nor_expression_fails(
    tmp_path: Path,
) -> None:
    rel = _release(["3.9"]) + (
        "  odd:\n    strategy:\n      matrix:\n        python: cp39\n"
    )
    r = _run(tmp_path, ">=3.9", ["3.9"], release=rel)
    assert r.returncode == 1
    assert "job odd: matrix python is 'cp39'" in r.stderr
