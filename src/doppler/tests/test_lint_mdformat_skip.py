"""`make lint-mdformat` skips for an old PYTHON, never for a tool that broke.

It used to ask `mdformat --version` and skip with "unavailable" when that
failed. A venv with a missing dependency (`mdurl`, or the mkdocs plugin) made
the tool fail to START, so the target returned 0 and `make lint` was green
locally while CI's mdformat rewrote the file (doppler#2857:
docs/quickstart.md).
Absent output is not a pass: on Python >= 3.10 a tool that cannot run FAILS.

`MDFORMAT=false` stands in for a tool that is broken or finds a problem. The
target must be red, and must not say it skipped.
"""

from __future__ import annotations

import subprocess
import sys

import pytest

from doppler.tests._platform import posix_only
from doppler.tests._repo import repo_root

REPO = repo_root(__file__)
pytestmark = [
    posix_only("runs make on the Makefile's shell recipe"),
    pytest.mark.skipif(
        sys.version_info < (3, 10),
        reason="mdformat is legitimately skipped on Python < 3.10",
    ),
]


def test_a_broken_mdformat_is_a_failure_not_a_skip() -> None:
    r = subprocess.run(
        ["make", "-s", "-C", str(REPO), "lint-mdformat", "MDFORMAT=false"],
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode != 0, r.stdout + r.stderr
    assert "skipping" not in r.stdout + r.stderr
