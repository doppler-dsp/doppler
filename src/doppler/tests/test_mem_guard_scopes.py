"""`scripts/mem-guard.sh`: whose ceiling is whose, and where the probe dies.

A defect about WHERE a limit lands:

- #1960: the shared `doppler-guard.slice` took its ceiling from whichever
  guarded command started last, so one caller's `MEM_GUARD_MAX=1G` held
  every other session's live command to 1G. The slice ceiling is now
  always 3/4 of MemTotal, and a caller's `MEM_GUARD_MAX` is a `MemoryMax`
  on its OWN scope.

The real behaviour needs a systemd user session, which CI runners do not
have; it was measured on a box (see the PR). These tests run the script
against stand-in `systemctl` and `systemd-run` commands that record every
call, so they pin the CONTRACT everywhere: which unit gets which
MemoryMax.
"""

from __future__ import annotations

import os
import subprocess
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "mem-guard.sh"

# One stand-in for both commands: log the call, then behave as the script
# needs. The probe (any run in the probe slice) "dies of the ceiling", exit
# 137; any other systemd-run executes what follows `--`.
SHIM = """#!/usr/bin/env bash
printf '%s\\n' "$(basename "$0") $*" >> "$MEM_GUARD_SHIM_LOG"
[ "$(basename "$0")" = systemctl ] && exit 0
case " $* " in *mgprobe*|*guard-probe*) exit 137 ;; esac
while [ "$1" != "--" ]; do shift; done
shift
exec "$@"
"""

pytestmark = pytest.mark.skipif(
    not os.path.exists("/proc/meminfo"),
    reason="the shared ceiling is derived from /proc/meminfo",
)


def _run(tmp_path: Path, **env: str) -> list[str]:
    """Run `mem-guard.sh true` against the shims; return the calls."""
    bin_dir = tmp_path / "bin"
    bin_dir.mkdir()
    for name in ("systemctl", "systemd-run"):
        shim = bin_dir / name
        shim.write_text(SHIM, encoding="utf-8")
        shim.chmod(0o755)
    log = tmp_path / "calls.log"
    full = {
        **os.environ,
        "PATH": f"{bin_dir}:{os.environ['PATH']}",
        "MEM_GUARD_SHIM_LOG": str(log),
        **env,
    }
    full.pop("MEM_GUARD", None)  # an inherited 0 would skip the guard
    if "MEM_GUARD_MAX" not in env:  # nor may an inherited cap leak in
        full.pop("MEM_GUARD_MAX", None)
    r = subprocess.run(
        ["bash", str(SCRIPT), "true"], env=full, capture_output=True, text=True
    )
    assert r.returncode == 0, r.stderr
    return log.read_text(encoding="utf-8").splitlines()


def _machine_ceiling() -> str:
    with open("/proc/meminfo", encoding="utf-8") as fh:
        kib = next(
            int(ln.split()[1]) for ln in fh if ln.startswith("MemTotal")
        )
    return f"{kib * 3 // 4 // 1024}M"


def _guard_slice_ceiling(calls: list[str]) -> str:
    [line] = [
        c
        for c in calls
        if c.startswith("systemctl ") and "doppler-guard.slice" in c
    ]
    return next(w for w in line.split() if w.startswith("MemoryMax="))


def _command_run(calls: list[str]) -> str:
    """The systemd-run that runs the guarded command itself."""
    runs = [c for c in calls if c.startswith("systemd-run ")]
    return next(c for c in runs if "--slice=doppler-guard.slice" in c)


def test_a_callers_cap_never_reaches_the_shared_slice(tmp_path: Path) -> None:
    """#1960: MEM_GUARD_MAX=1G leaves the slice at 3/4 of MemTotal."""
    calls = _run(tmp_path, MEM_GUARD_MAX="1G")
    assert _guard_slice_ceiling(calls) == f"MemoryMax={_machine_ceiling()}"


def test_a_callers_cap_lands_on_its_own_scope(tmp_path: Path) -> None:
    calls = _run(tmp_path, MEM_GUARD_MAX="1G")
    assert "-p MemoryMax=1G" in _command_run(calls)


def test_without_a_cap_the_scope_has_no_limit_of_its_own(
    tmp_path: Path,
) -> None:
    calls = _run(tmp_path)
    assert "MemoryMax" not in _command_run(calls)
    assert _guard_slice_ceiling(calls) == f"MemoryMax={_machine_ceiling()}"
