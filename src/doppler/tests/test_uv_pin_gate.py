"""The uv pin gates, exercised over seeded trees and a copied lock.

uv.lock's bytes depend on the uv that writes it: a version bump re-locked by
uv 0.11.16 changed 2 lines, by 0.11.28 308 -- the v0.65.0 release commit
(doppler#1940). So uv is pinned once, in pyproject.toml's ``[tool.uv]
required-version``, and two gates hold it:

- ``scripts/check_uv_pin.py`` (``make lint-uv-pin``): the pin is exact, and
  every uv installer reads it. Each case seeds a tree and points ``--root``
  at it.
- ``make lint-uv-lock``: the committed uv.lock is exactly what the pinned uv
  writes. Each case copies the real lock into a scratch project and runs the
  real target there through ``UV="uv --directory <copy>"``, so the test
  exercises the Makefile's own LOCK_CMD rather than a restatement of it.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_uv_pin.py"
COMPOSITE = ".github/actions/setup-uv/action.yml"
DOCKERFILE = "deploy/docker/Dockerfile"

PIN = '[tool.uv]\nrequired-version = "==0.12.24"\n'
GOOD_COMPOSITE = """\
runs:
  using: composite
  steps:
    - name: attempt 1
      uses: astral-sh/setup-uv@v9.0.0
      with:
        version-file: pyproject.toml
"""


def _seed(
    tmp_path: Path,
    pyproject: str = PIN,
    composite: str | None = GOOD_COMPOSITE,
    files: dict[str, str] | None = None,
) -> Path:
    tmp_path.mkdir(parents=True, exist_ok=True)
    (tmp_path / "pyproject.toml").write_text(
        '[project]\nname = "x"\n\n' + pyproject, encoding="utf-8"
    )
    if composite is not None:
        f = tmp_path / COMPOSITE
        f.parent.mkdir(parents=True)
        f.write_text(composite, encoding="utf-8")
    for rel, text in (files or {}).items():
        f = tmp_path / rel
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_text(text, encoding="utf-8")
    return tmp_path


def _run(root: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root), *args],
        capture_output=True,
        text=True,
    )


# ── rule 1: the pin itself ───────────────────────────────────────────────────


def test_an_exact_pin_and_a_clean_tree_pass(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path))
    assert r.returncode == 0, r.stdout
    assert "uv 0.12.24" in r.stdout


@pytest.mark.parametrize(
    ("pyproject", "said"),
    [
        ("", "no [tool.uv] required-version"),
        # A range is not a pin: 0.11.16 and 0.11.28 both satisfy this one
        # and write different lock bytes.
        ('[tool.uv]\nrequired-version = ">=0.11,<0.12"\n', "not an exact"),
        ('[tool.uv]\nrequired-version = "0.12.24"\n', "not an exact"),
        # The key under some other table is no pin at all.
        ('[tool.other]\nrequired-version = "==0.12.24"\n', "no [tool.uv]"),
    ],
)
def test_a_missing_or_inexact_pin_is_refused(
    tmp_path: Path, pyproject: str, said: str
) -> None:
    r = _run(_seed(tmp_path, pyproject=pyproject))
    assert r.returncode == 1, r.stdout
    assert said in r.stdout


def test_a_uv_toml_pin_is_a_second_declaration(tmp_path: Path) -> None:
    """uv.toml outranks pyproject.toml for uv and setup-uv alike."""
    files = {"uv.toml": 'required-version = "==0.11.28"\n'}
    r = _run(_seed(tmp_path, files=files))
    assert r.returncode == 1, r.stdout
    assert "uv.toml" in r.stdout


def test_print_version_is_the_makefiles_reader(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path), "--print-version")
    assert (r.returncode, r.stdout) == (0, "0.12.24\n")
    bad = _run(_seed(tmp_path / "b", pyproject=""), "--print-version")
    assert bad.returncode == 1
    assert bad.stdout == ""


# ── rule 2: setup-uv has one home, and it names the pin file ─────────────────


def test_a_direct_setup_uv_in_a_workflow_is_named(tmp_path: Path) -> None:
    wf = (
        "jobs:\n  a:\n    steps:\n"
        "      - uses: actions/checkout@v7\n"
        "      - name: Install uv\n        uses: astral-sh/setup-uv@v7\n"
    )
    r = _run(_seed(tmp_path, files={".github/workflows/w.yml": wf}))
    assert r.returncode == 1, r.stdout
    assert ".github/workflows/w.yml: step 'Install uv' uses" in r.stdout


@pytest.mark.parametrize(
    ("with_block", "said"),
    [
        # setup-uv's own search falls back to the latest uv on a missing pin.
        ("      with:\n        python-version: '3.12'\n", "lacks"),
        ("", "lacks"),
        (
            "      with:\n        version-file: pyproject.toml\n"
            "        version: 0.12.24\n",
            "second copy",
        ),
    ],
)
def test_a_composite_step_must_name_the_pin_file(
    tmp_path: Path, with_block: str, said: str
) -> None:
    composite = (
        "runs:\n  using: composite\n  steps:\n"
        "    - name: attempt 1\n      uses: astral-sh/setup-uv@v9.0.0\n"
        + with_block
    )
    r = _run(_seed(tmp_path, composite=composite))
    assert r.returncode == 1, r.stdout
    assert said in r.stdout


def test_a_composite_that_installs_no_uv_is_refused(tmp_path: Path) -> None:
    composite = "runs:\n  using: composite\n  steps:\n    - run: true\n"
    r = _run(_seed(tmp_path, composite=composite))
    assert r.returncode == 1, r.stdout
    assert "installs no uv" in r.stdout


# ── rule 3: every other installer reads UV_VERSION ───────────────────────────


@pytest.mark.parametrize(
    ("rel", "text"),
    [
        # The manylinux leg's line, joined across its continuation.
        (
            ".github/workflows/r.yml",
            "x: |\n  $PYTHON -m pip install --quiet numpy \\\n    uv build\n",
        ),
        ("scripts/a.sh", "pip3 install 'uv>=0.12'\n"),
        # Escaped inside a double-quoted `bash -c "..."`.
        ("scripts/a.sh", 'bash -c "pip install \\"uv\\" build"\n'),
        ("scripts/a.sh", "pip install uv==0.12.24\n"),  # a literal copy
        ("scripts/a.sh", "pipx install uv\n"),
        ("scripts/a.sh", "uv tool install uv\n"),
        ("scripts/a.sh", "pip install uv; echo done\n"),
        ("scripts/a.sh", "uv self update\n"),
        (DOCKERFILE, "RUN curl -fsSL https://astral.sh/uv/install.sh | sh\n"),
        (
            DOCKERFILE,
            "RUN curl -fsSL https://astral.sh/uv/0.12.24/install.sh\n",
        ),
        (DOCKERFILE, "COPY --from=ghcr.io/astral-sh/uv:latest /uv /bin/\n"),
        (DOCKERFILE, "ARG UV_VERSION=0.12.24\n"),
    ],
)
def test_an_installer_that_does_not_read_the_pin_is_named(
    tmp_path: Path, rel: str, text: str
) -> None:
    r = _run(_seed(tmp_path, files={rel: text}))
    assert r.returncode == 1, r.stdout
    assert f"{rel}:" in r.stdout


@pytest.mark.parametrize(
    "text",
    [
        "pip install uv==${UV_VERSION}\n",
        # The manylinux leg's form: escaped for the host shell.
        'bash -euxc "pip install numpy uv==\\${UV_VERSION:?}"\n',
        'pip install "uv==$(UV_VERSION)"\n',
        'RUN : "${UV_VERSION:?}" \\\n && curl -fsSL '
        '"https://astral.sh/uv/${UV_VERSION}/install.sh" | sh\n',
        "COPY --from=ghcr.io/astral-sh/uv:${UV_VERSION} /uv /bin/\n",
        "uv self update ${UV_VERSION}\n",
        "ARG UV_VERSION\n",
        # Not uv: a package whose name merely starts with it, and uv used
        # to install something else.
        "pip install uvicorn uv-dynamic-versioning\n",
        'uv tool install --python python3 "just-makeit==${JM_VERSION}"\n',
        "# pip install uv -- a comment\n",
    ],
)
def test_an_installer_that_reads_the_pin_or_a_non_installer_passes(
    tmp_path: Path, text: str
) -> None:
    r = _run(_seed(tmp_path, files={DOCKERFILE: text}))
    assert r.returncode == 0, r.stdout


def test_prose_is_not_in_scope(tmp_path: Path) -> None:
    """A docs page may show `uv self update 0.12.24` to a reader."""
    files = {"docs/setup.md": "uv self update 0.12.24\n", "README.md": "x"}
    r = _run(_seed(tmp_path, files=files))
    assert r.returncode == 0, r.stdout


def test_the_live_tree_passes() -> None:
    r = subprocess.run(
        [sys.executable, str(SCRIPT)], capture_output=True, text=True
    )
    assert r.returncode == 0, r.stdout


# ── the artifact: uv.lock is the pinned uv's own output ──────────────────────


def _lock_gate(tmp_path: Path, edit: tuple[str, str] | None = None):
    """Run the real `make lint-uv-lock` against a copy of the project."""
    for name in ("pyproject.toml", "uv.lock", "README.md"):
        shutil.copy(REPO / name, tmp_path / name)
    if edit is not None:
        lock = tmp_path / "uv.lock"
        old, new = edit
        text = lock.read_text(encoding="utf-8")
        assert old in text, f"seed edit {old!r} no longer applies"
        lock.write_text(text.replace(old, new, 1), encoding="utf-8")
    # The uv that launched this test: `uv run` exports its own path as UV,
    # and under the pin no other uv could have run it. PATH is the fallback.
    uv = os.environ.get("UV") or shutil.which("uv")
    assert uv, "no uv found; run this through make"
    return subprocess.run(
        [
            "make",
            "-s",
            "-C",
            str(REPO),
            "lint-uv-lock",
            f"UV={uv} --directory {tmp_path}",
        ],
        capture_output=True,
        text=True,
    )


def test_the_committed_lock_is_the_pinned_uvs_output(tmp_path: Path) -> None:
    r = _lock_gate(tmp_path)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "lint-uv-lock: OK" in r.stdout


@pytest.mark.parametrize(
    "edit",
    [
        # An older uv's format revision: 0.11.x writes 3, 0.12.x writes 5.
        ("\nrevision = 5\n", "\nrevision = 3\n"),
        # The marker spelling #1940 is about: 0.11.16 spelled out a marker
        # the resolution fork already implies, and still resolves the same.
        (
            '{ name = "zipp" },',
            '{ name = "zipp", marker = "python_full_version < \'3.10\'" },',
        ),
    ],
)
def test_a_lock_no_pinned_uv_wrote_is_refused(
    tmp_path: Path, edit: tuple[str, str]
) -> None:
    r = _lock_gate(tmp_path, edit)
    assert r.returncode != 0, r.stdout
    assert "is not what uv" in r.stdout
    # Read-only: the probe must not have repaired the copy behind our back.
    assert edit[1] in (tmp_path / "uv.lock").read_text(encoding="utf-8")
