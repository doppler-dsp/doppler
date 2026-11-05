"""The docs refuse Python 3.10 in words, not with a missing command."""

from __future__ import annotations

import subprocess
import sys

from doppler.tests._repo import repo_root

SCRIPT = str(repo_root(__file__) / "scripts" / "check_docs_python.py")


def _run(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, SCRIPT, *args],
        capture_output=True,
        text=True,
        check=False,
    )


def test_python_310_is_refused_with_the_reason() -> None:
    r = _run("--version", "3.10")
    assert r.returncode == 1
    assert "3.11" in r.stderr and "cannot build the docs" in r.stderr


def test_python_311_and_newer_pass() -> None:
    assert _run("--version", "3.11").returncode == 0
    assert _run("--version", "3.14").returncode == 0
