"""The docs build is held to a peak-memory budget, and says so where CI looks.

A cold strict build of the 918 pages peaks near 500 MB with the matched
toolchain pyproject.toml's docs group pins (511 MB on zensical 0.1.1,
2026-11-10).
With zensical 0.0.45 and mkdocstrings 1.0.4 it grew past 4 GB and was killed 9
times at the shared guard's 7 GB ceiling (#2840), each as a silent `docs-check`
failure.

Two holds, and only one of them reaches CI:

* the BUDGET (`scripts/peak_rss.py`, `DOCS_PEAK_MB`): reads `getrusage`, so it
  works on a GitHub runner. `docs-check`'s build line goes through it.
* the CEILING (`MEM_GUARD_MAX`, `DOCS_MEM_MAX`): LOCAL-ONLY. `mem-guard.sh`
  runs a command unguarded when there is no systemd user session, which is
  what a runner has.

The cases below hold that the build line carries the budget and the local
ceiling exists; `test_peak_rss.py` proves the wrapper fails an over-budget run.
Sabotage: `make docs-check DOCS_PEAK_MB=300` fails the build.
"""

from __future__ import annotations

import re
import subprocess
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
MAX_MB = 3 * 1024


def _var(tmp_path: Path, name: str) -> str:
    probe = tmp_path / "probe.mk"
    probe.write_text(f"show:\n\t@echo '$({name})'\n", encoding="utf-8")
    return subprocess.run(
        [
            "make",
            "-s",
            "-C",
            str(REPO),
            "-f",
            "Makefile",
            "-f",
            str(probe),
            "show",
        ],
        capture_output=True,
        text=True,
        check=True,
    ).stdout


def test_the_docs_check_build_goes_through_the_peak_rss_budget(
    tmp_path: Path,
) -> None:
    cmd = _var(tmp_path, "DOCS_CHECK_BUILD_CMD")
    m = re.search(r"scripts/peak_rss\.py --max-mb (\d+) -- ", cmd)
    assert m, f"docs-check's build is not wrapped by peak_rss.py: {cmd!r}"
    assert int(m[1]) <= MAX_MB, f"budget {m[1]} MB is above {MAX_MB} MB"
    assert "zensical" in cmd and "--strict" in cmd


def test_the_local_ceiling_exists_and_is_low(tmp_path: Path) -> None:
    cmd = _var(tmp_path, "ZENSICAL")
    m = re.search(r"MEM_GUARD_MAX=(\d+)([MG])\b", cmd)
    assert m, f"ZENSICAL names no MEM_GUARD_MAX ceiling: {cmd!r}"
    mb = int(m[1]) * (1024 if m[2] == "G" else 1)
    assert mb <= MAX_MB, f"ceiling {m[0]} is above 3G"
