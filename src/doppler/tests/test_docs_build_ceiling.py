"""The docs build runs under its own memory ceiling, far below the shared one.

A cold strict build of the 918 pages peaks near 520 MB with the matched
toolchain (zensical 0.0.74, mkdocstrings 1.0.8, mkdocstrings-python 2.0.10).
With zensical 0.0.45 and mkdocstrings 1.0.4 it grew past 4 GB (about 6 MB per
page held until the last stage) and was killed 9 times at the shared guard's
7 GB ceiling (#2840), each time as a silent `docs-check` failure.

The ceiling is what makes a return of that into a fast failure on the one
command, so this holds it: `ZENSICAL` carries a `MEM_GUARD_MAX` of no more
than 3G (about six times the measured peak, less than half the shared one).
Sabotage: `make docs-check DOCS_MEM_MAX=300M` fails the build.
"""

from __future__ import annotations

import re
import subprocess
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
CEILING_MAX_MB = 3 * 1024


def test_the_docs_build_has_its_own_memory_ceiling(tmp_path: Path) -> None:
    probe = tmp_path / "probe.mk"
    probe.write_text(
        "zensical-cmd:\n\t@echo '$(ZENSICAL)'\n", encoding="utf-8"
    )
    r = subprocess.run(
        [
            "make",
            "-s",
            "-C",
            str(REPO),
            "-f",
            "Makefile",
            "-f",
            str(probe),
            "zensical-cmd",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    m = re.search(r"MEM_GUARD_MAX=(\d+)([MG])\b", r.stdout)
    assert m, f"ZENSICAL names no MEM_GUARD_MAX ceiling: {r.stdout!r}"
    mb = int(m[1]) * (1024 if m[2] == "G" else 1)
    assert mb <= CEILING_MAX_MB, f"ceiling {m[0]} is above 3G"
