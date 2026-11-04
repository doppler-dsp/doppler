"""What `make` exports to every recipe stays small.

GNU xargs on msys refuses to start when the environment plus the command
line leave less than LINE_MAX of argument space (`assertion
"bc_ctl.arg_max >= LINE_MAX" failed`). `make lint-clang-format` pipes a file
list into xargs, so on the Windows leg anything `make` EXPORTS comes straight
out of that space.

It went red once without anyone touching the file list: `standard.mk` exports
`DOCS_CHECK_PRE_CMDS` to every recipe, and the Makefile built that value with
`$(GALLERY_SCRIPTS)` expanded into it (about 1.9 KB). The Windows runner sat
within a hundred bytes of the limit, so one more gate line in the list
(#2829, +99 bytes) tipped it. The fix is where the value is built: the line
calls a make target, which keeps the script list in its own argv, not in the
environment of every other recipe.

The case runs `make` with an otherwise EMPTY environment, so what it counts
is exactly what the Makefile exports, on any box.
"""

from __future__ import annotations

import os
import subprocess
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
# Measured 2762 bytes (2026-11-04) with the list kept out; 5159 with it in.
TOTAL_MAX = 4000
VAR_MAX = 2600


def test_the_environment_make_exports_is_small(tmp_path: Path) -> None:
    probe = tmp_path / "probe.mk"
    probe.write_text("envdump:\n\t@env -0\n", encoding="utf-8")
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
            "envdump",
        ],
        capture_output=True,
        env={"PATH": os.environ["PATH"], "HOME": str(tmp_path)},
        check=True,
    )
    entries = [e for e in r.stdout.split(b"\0") if e]
    total = sum(len(e) + 1 for e in entries)
    biggest = max(entries, key=len)
    assert total <= TOTAL_MAX, (
        f"make exports {total} bytes (budget {TOTAL_MAX}); the largest is "
        f"{biggest[:60]!r} at {len(biggest)}"
    )
    assert len(biggest) <= VAR_MAX, (
        f"{biggest[:60]!r} is {len(biggest)} bytes (budget {VAR_MAX})"
    )
