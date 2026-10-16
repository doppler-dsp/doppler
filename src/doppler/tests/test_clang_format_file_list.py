"""`make lint-clang-format` fails when it cannot list the files it checks.

Its file list has two halves: every tracked C file outside the generated
set, plus the HAND-WRITTEN `_ext.c` of each `no_generate` module, which
`scripts/list_hand_ext_c.py` derives from the manifest (doppler#1432). The
second half used to run as a bare `uv run` inside a `{ ...; }` group piped
into xargs. POSIX sh has no pipefail, so when that script failed -- on any
box whose uv is not the pin -- the list just lost those files and the
target exited 0 (#2361). A gate reading green while checking less. (With
the default `UV` on such a box, the clang-format call is refused too and
xargs fails the target; the exit 0 needed `UV=` naming the pinned uv, which
the bare listing ignored, or a listing that failed some other way.)

The fake `UV` below fails ONLY the listing step and turns the clang-format
step into a no-op, so the case cannot reformat the tree. It catches both
halves of the old defect: a bare `uv` never reaches the fake, so the listing
succeeds and the target exits 0; a `$(UV)` whose failure is lost in the
pipe also exits 0. Only a target that runs `$(UV)` AND stops on its failure
goes red here.

The second case is the other direction: with a working listing, every file
it names reaches clang-format. Dropping the hand-written half from the
pipe -- the "checking fewer files" regression itself -- passes the first
case and fails this one.
"""

from __future__ import annotations

import os
import shutil
import stat
import subprocess
from pathlib import PurePath
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
        [
            "make",
            "-s",
            "-C",
            str(REPO),
            "lint-clang-format",
            # POSIX spelling: the recipe runs under make's shell, which eats
            # a Windows path's backslashes (test_uv_pin_gate.py says why).
            f"UV={PurePath(uv).as_posix()}",
        ],
        capture_output=True,
        text=True,
        env={**os.environ, "MAKEFLAGS": ""},
    )
    assert r.returncode != 0, (
        "lint-clang-format exited 0 although the hand-written _ext.c list "
        "could not be produced:\n" + r.stdout + r.stderr
    )
    assert "fake" in r.stderr, r.stdout + r.stderr


# Runs the REAL listing through the uv that launched this test, and logs
# every file the clang-format call is handed instead of formatting it.
LOGGING_UV = """\
#!/bin/sh
case "$*" in
  *list_hand_ext_c.py*)
    exec "$REAL_UV" "$@" ;;
  *clang-format*)
    for a in "$@"; do
      case "$a" in *.c|*.h) printf '%s\\n' "$a" >> "$CF_LOG" ;; esac
    done ;;
esac
exit 0
"""


def test_every_listed_hand_written_file_reaches_clang_format(
    tmp_path: Path,
) -> None:
    real = os.environ.get("UV") or shutil.which("uv")
    assert real, "no uv found; run this through make"
    uv = tmp_path / "uv"
    uv.write_text(LOGGING_UV, encoding="utf-8")
    uv.chmod(uv.stat().st_mode | stat.S_IXUSR)
    log = tmp_path / "cf.log"
    r = subprocess.run(
        [
            "make",
            "-s",
            "-C",
            str(REPO),
            "lint-clang-format",
            f"UV={PurePath(uv).as_posix()}",
        ],
        capture_output=True,
        text=True,
        env={
            **os.environ,
            "MAKEFLAGS": "",
            "REAL_UV": PurePath(real).as_posix(),
            "CF_LOG": PurePath(log).as_posix(),
        },
    )
    assert r.returncode == 0, r.stdout + r.stderr
    hand = subprocess.run(
        [real, "run", "python", "scripts/list_hand_ext_c.py"],
        cwd=REPO,
        capture_output=True,
        text=True,
        check=True,
    ).stdout.split()
    assert hand, "the manifest names no hand-written _ext.c to check"
    seen = set(log.read_text(encoding="utf-8").split())
    missing = [f for f in hand if f not in seen]
    assert not missing, f"listed but never handed to clang-format: {missing}"
    # The tracked half reaches it too: not just the hand-written files.
    assert len(seen) > len(hand), sorted(seen)
