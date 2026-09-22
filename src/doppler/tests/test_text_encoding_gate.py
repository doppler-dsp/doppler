"""The text-encoding gate, exercised over a seeded tree.

`scripts/check_text_encoding.py` exists because a bare ``read_text()``
decodes with the locale codec -- cp1252 on Windows -- and the first Windows
run of the suite lost three test modules to it at collection (doppler#1457).
Each case seeds a fake ``src/doppler`` and points ``--root`` at it, because
a gate that can only run against the real tree cannot be sabotaged.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

SCRIPT = repo_root(__file__) / "scripts" / "check_text_encoding.py"

#: The bare call, assembled so this file is not itself a match for the gate
#: it tests (the gate scans every .py under src/doppler, tests included).
BARE = "read_text(" + ")"


def _run(tmp_path: Path, body: str) -> subprocess.CompletedProcess[str]:
    pkg = tmp_path / "src" / "doppler"
    pkg.mkdir(parents=True)
    (pkg / "mod.py").write_text(body, encoding="utf-8")
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
        capture_output=True,
        text=True,
        encoding="utf-8",
    )


def test_a_bare_read_fails_and_is_named(tmp_path: Path) -> None:
    r = _run(tmp_path, f"x = p.{BARE}\n")
    assert r.returncode == 1, r.stdout
    assert "src/doppler/mod.py:1" in r.stdout


def test_an_explicit_encoding_passes(tmp_path: Path) -> None:
    r = _run(tmp_path, 'x = p.read_text(encoding="utf-8")\n')
    assert r.returncode == 0, r.stdout


def test_a_comment_about_the_call_passes(tmp_path: Path) -> None:
    r = _run(tmp_path, f"# never write p.{BARE} here\n")
    assert r.returncode == 0, r.stdout


def test_the_real_tree_passes() -> None:
    r = subprocess.run(
        [sys.executable, str(SCRIPT)],
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    assert r.returncode == 0, r.stdout
