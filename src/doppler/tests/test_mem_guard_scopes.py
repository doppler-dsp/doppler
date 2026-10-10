"""`scripts/mem-guard.sh`: whose ceiling is whose, and where the probe dies.

A defect about WHERE a limit lands:

- #1960: the shared `doppler-guard.slice` took its ceiling from whichever
  guarded command started last, so one caller's `MEM_GUARD_MAX=1G` held
  every other session's live command to 1G. The slice ceiling is now
  always 3/4 of MemTotal, and a caller's `MEM_GUARD_MAX` is a `MemoryMax`
  on its OWN scope. A cap systemd-run refuses is dropped for the shared
  ceiling, so a typo cannot stop the command running.

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
# needs. A MemoryMax systemd-run cannot parse is refused, as the real one
# does (measured: `-p MemoryMax=4GB` exits 1, "Failed to parse"). The probe
# (any run in the probe slice) "dies of the ceiling", exit 137; any other
# systemd-run executes what follows `--`, and one without `--` is an error
# rather than a loop past the end of its arguments.
BAD_CAP = "4GB"
SHIM = f"""#!/usr/bin/env bash
printf '%s\\n' "$(basename "$0") $*" >> "$MEM_GUARD_SHIM_LOG"
[ "$(basename "$0")" = systemctl ] && exit 0
case " $* " in *" MemoryMax={BAD_CAP} "*)
  echo "Failed to parse MemoryMax={BAD_CAP}: Invalid argument" >&2
  exit 1 ;;
esac
case " $* " in *mgprobe*|*guard-probe*) exit 137 ;; esac
while [ "${{1-}}" != "--" ]; do
  if [ $# -eq 0 ]; then
    echo "systemd-run: no -- before the command" >&2
    exit 2
  fi
  shift
done
shift
exec "$@"
"""

# The guarded command: it leaves a file behind, so a run that logs the
# command and never executes it fails. The path goes through the
# environment, keeping the logged command line free of tmp_path (whose
# name is the test's, and could match whatever a test greps the log for).
COMMAND = ["sh", "-c", 'echo ran > "$MEM_GUARD_TEST_MARKER"']

pytestmark = pytest.mark.skipif(
    not os.path.exists("/proc/meminfo"),
    reason="the shared ceiling is derived from /proc/meminfo",
)


def _run(tmp_path: Path, **env: str) -> tuple[list[str], str]:
    """Run `mem-guard.sh COMMAND` against the shims.

    Returns the logged calls and the script's stderr. Fails unless the
    script exits 0 AND the command ran: the guard never blocks the work.
    """
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
        "MEM_GUARD_TEST_MARKER": str(tmp_path / "ran"),
        **env,
    }
    full.pop("MEM_GUARD", None)  # an inherited 0 would skip the guard
    if "MEM_GUARD_MAX" not in env:  # nor may an inherited cap leak in
        full.pop("MEM_GUARD_MAX", None)
    r = subprocess.run(
        ["bash", str(SCRIPT), *COMMAND],
        env=full,
        capture_output=True,
        text=True,
        timeout=60,
    )
    assert r.returncode == 0, r.stderr
    assert (tmp_path / "ran").is_file(), "the guarded command never ran"
    return log.read_text(encoding="utf-8").splitlines(), r.stderr


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
    """The one systemd-run that runs the guarded command, in the guard."""
    [line] = [
        c
        for c in calls
        if c.startswith("systemd-run ") and "MEM_GUARD_TEST_MARKER" in c
    ]
    assert "--slice=doppler-guard.slice" in line, line
    return line


def test_a_callers_cap_never_reaches_the_shared_slice(tmp_path: Path) -> None:
    """#1960: MEM_GUARD_MAX=1G leaves the slice at 3/4 of MemTotal."""
    calls, _ = _run(tmp_path, MEM_GUARD_MAX="1G")
    assert _guard_slice_ceiling(calls) == f"MemoryMax={_machine_ceiling()}"


def test_a_callers_cap_lands_on_its_own_scope(tmp_path: Path) -> None:
    calls, _ = _run(tmp_path, MEM_GUARD_MAX="1G")
    run = _command_run(calls)
    assert "-p MemoryMax=1G" in run
    assert "-p MemorySwapMax=0" in run  # else the scope swaps past its cap


def test_without_a_cap_the_scope_has_no_limit_of_its_own(
    tmp_path: Path,
) -> None:
    calls, _ = _run(tmp_path)
    assert "MemoryMax" not in _command_run(calls)
    assert _guard_slice_ceiling(calls) == f"MemoryMax={_machine_ceiling()}"


def test_a_refused_cap_falls_back_to_the_shared_ceiling(
    tmp_path: Path,
) -> None:
    """A cap systemd-run will not parse must not stop the command.

    `exec systemd-run -p MemoryMax=4GB` exits 1 before the command starts.
    The fallback is the guarded scope WITHOUT a cap of its own, not an
    unguarded run: the shared ceiling has been proved by then.
    """
    calls, stderr = _run(tmp_path, MEM_GUARD_MAX=BAD_CAP)  # asserts it ran
    # The script's own line, not systemd-run's "Failed to parse", which
    # names the cap too.
    [fallback] = [ln for ln in stderr.splitlines() if "falling back" in ln]
    assert f"MemoryMax={BAD_CAP}" in fallback, stderr
    assert "MemoryMax" not in _command_run(calls)  # in the guard, uncapped
    assert _guard_slice_ceiling(calls) == f"MemoryMax={_machine_ceiling()}"
