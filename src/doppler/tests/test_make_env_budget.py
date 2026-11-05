"""What the Makefile exports to every recipe stays small.

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

The case runs a recipe that dumps its environment twice, once under a bare
makefile and once under the repo's, and counts what the Makefile ADDED. A
total would not do: PATH alone is 4.5 KB on the Windows runner (a first
version of this test failed there for exactly that reason).
"""

from __future__ import annotations

import os
import subprocess
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
# What the Makefile ADDS to a recipe's environment, over what `make` and the
# platform put there anyway (PATH alone is 4.5 KB on the Windows runner, which
# is why the budget is a difference). Measured 2026-11-04 on Linux: 2556 bytes
# with the list kept out, 4980 with it in; the largest variable 2548 then.
ADDED_MAX = 3500
VAR_MAX = 1800


def _env(cwd: Path, files: list[str], home: Path) -> dict[bytes, bytes]:
    r = subprocess.run(
        [
            "make",
            "-s",
            "-C",
            str(cwd),
            *[a for f in files for a in ("-f", f)],
            "envdump",
        ],
        capture_output=True,
        env={"PATH": os.environ["PATH"], "HOME": str(home)},
        check=True,
    )
    out: dict[bytes, bytes] = {}
    for e in r.stdout.split(b"\0"):
        k, _, v = e.partition(b"=")
        if k:
            out[k] = v
    return out


def test_the_environment_the_makefile_exports_is_small(
    tmp_path: Path,
) -> None:
    probe = tmp_path / "probe.mk"
    probe.write_text("envdump:\n\t@env -0\n", encoding="utf-8")
    bare = tmp_path / "bare"
    bare.mkdir()
    base = _env(bare, [str(probe)], tmp_path)
    full = _env(REPO, ["Makefile", str(probe)], tmp_path)
    added = {k: v for k, v in full.items() if base.get(k) != v}
    total = sum(len(k) + len(v) + 2 for k, v in added.items())
    big = max(added, key=lambda k: len(added[k]))
    assert total <= ADDED_MAX, (
        f"the Makefile exports {total} bytes to every recipe (budget "
        f"{ADDED_MAX}); the largest is {big!r} at {len(added[big])}"
    )
    assert len(added[big]) <= VAR_MAX, (
        f"{big!r} is {len(added[big])} bytes (budget {VAR_MAX})"
    )
