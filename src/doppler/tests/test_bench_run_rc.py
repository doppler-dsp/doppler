"""`make bench-interleaved` leaves its exit code where a watcher can wait.

The release bench run takes most of an hour and is launched in the
background over ssh. Its only outward signal used to be the process, and a
remote ``pgrep -f`` always matches the ssh shell that carries the pattern,
so a watcher read "running" after the run had already FAILED -- v0.77.0's
run sat unnoticed for hours that way (#2681, #2675).

The target now writes ``<VERSION>.log`` and, last, ``<VERSION>.rc`` under
``BENCH_RUN_DIR``. These tests drive the real recipe with ``UV`` swapped for
a stub, so nothing is built or measured: they hold that the file holds the
run's exact code on failure and on success, that make's own exit agrees,
and that a previous run's file is gone before the new run starts.
"""

from __future__ import annotations

import os
import stat
import subprocess
from typing import TYPE_CHECKING

import pytest

from doppler.tests._platform import posix_only
from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

# The recipe is POSIX sh and UV is swapped for a `#!/bin/sh` stub, which
# make on Windows cannot exec; bench-interleaved itself runs only on a POSIX
# bench box (cpufreq, git worktrees).
pytestmark = posix_only("execs a #!/bin/sh stub through a POSIX-sh recipe")

REPO = repo_root(__file__)
VERSION = "9.9.9"

#: Stands in for uv: says whether a stale .rc is visible while the run is
#: in progress, then exits with $CODE.
STUB = """#!/bin/sh
[ -e "$RC" ] && echo "stale-rc-visible"
echo "stub ran: $*"
exit "$CODE"
"""


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8").strip()


def _run(tmp_path: Path, code: int) -> subprocess.CompletedProcess[str]:
    stub = tmp_path / "uv"
    stub.write_text(STUB, encoding="utf-8")
    stub.chmod(stub.stat().st_mode | stat.S_IXUSR)
    runs = tmp_path / "runs"
    env = {
        **os.environ,
        "RC": str(runs / f"{VERSION}.rc"),
        "CODE": str(code),
    }
    env.pop("MAKEFLAGS", None)
    return subprocess.run(
        [
            "make",
            "-s",
            "bench-interleaved",
            f"VERSION={VERSION}",
            f"UV={stub}",
            f"BENCH_RUN_DIR={runs}",
        ],
        cwd=REPO,
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )


@pytest.mark.parametrize("code", [0, 3])
def test_the_rc_file_holds_the_runs_exit_code(
    tmp_path: Path, code: int
) -> None:
    r = _run(tmp_path, code)
    runs = tmp_path / "runs"
    assert _read(runs / f"{VERSION}.rc") == str(code)
    assert (r.returncode == 0) == (code == 0), r.stderr
    log = _read(runs / f"{VERSION}.log")
    assert f"run python scripts/bench_interleaved.py {VERSION}" in log
    assert not (runs / f"{VERSION}.rc.tmp").exists()


def test_a_previous_runs_rc_is_gone_before_the_run_starts(
    tmp_path: Path,
) -> None:
    runs = tmp_path / "runs"
    runs.mkdir()
    (runs / f"{VERSION}.rc").write_text("0\n", encoding="utf-8")
    _run(tmp_path, 3)
    log = _read(runs / f"{VERSION}.log")
    assert "stale-rc-visible" not in log
    assert _read(runs / f"{VERSION}.rc") == "3"
