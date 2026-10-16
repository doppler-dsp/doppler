"""`make lint-clang-format` fails when it cannot list the files it checks.

Its file list has two halves: every tracked C file outside the generated
set, plus the HAND-WRITTEN `_ext.c` of each `no_generate` module, which
`scripts/list_hand_ext_c.py` derives from the manifest (doppler#1432). The
second half used to run as a bare `uv run` inside a `{ ...; }` group piped
into xargs. POSIX sh has no pipefail, so when that script failed -- on any
box whose uv is not the pin, every time -- the list just lost those files
and the target exited 0 (#2361). A gate reading green while checking less.

The fake `UV` below fails ONLY the listing step and turns the clang-format
step into a no-op, so the case cannot reformat the tree. It catches both
halves of the old defect: a bare `uv` never reaches the fake, so the listing
succeeds and the target exits 0; a `$(UV)` whose failure is lost in the
pipe also exits 0. Only a target that runs `$(UV)` AND stops on its failure
goes red here.
"""

from __future__ import annotations

import os
import stat
import subprocess
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)

FAKE_UV = """\
#!/bin/sh
# Fails the hand-written listing; every other uv call (clang-format via
# DEV_RUN) does nothing, so the tree is never touched.
case "$*" in
  *list_hand_ext_c.py*)
    echo "error: Required uv version does not match (fake)" >&2
    exit 2 ;;
esac
exit 0
"""


def test_a_failed_hand_written_listing_fails_the_target(
    tmp_path: Path,
) -> None:
    uv = tmp_path / "uv"
    uv.write_text(FAKE_UV, encoding="utf-8")
    uv.chmod(uv.stat().st_mode | stat.S_IXUSR)
    r = subprocess.run(
        ["make", "-s", "-C", str(REPO), "lint-clang-format", f"UV={uv}"],
        capture_output=True,
        text=True,
        env={**os.environ, "MAKEFLAGS": ""},
    )
    assert r.returncode != 0, (
        "lint-clang-format exited 0 although the hand-written _ext.c list "
        "could not be produced:\n" + r.stdout + r.stderr
    )
    assert "fake" in r.stderr, r.stdout + r.stderr
