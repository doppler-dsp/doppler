"""`scripts/peak_rss.py` fails a run whose peak is over budget.

It exists because the memory guard's ceiling is a no-op where there is no
systemd user session (a CI runner): the budget is what a runner sees.
"""

from __future__ import annotations

import subprocess
import sys

from doppler.tests._repo import repo_root

REPO = repo_root(__file__)
SCRIPT = str(REPO / "scripts" / "peak_rss.py")
# Holds ~200 MB resident for a moment, then exits 0.
BIG = "x = bytearray(200 * 1024 * 1024); x[::4096] = b'1' * len(x[::4096])"


def _run(max_mb: int, *cmd: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, SCRIPT, "--max-mb", str(max_mb), "--", *cmd],
        capture_output=True,
        text=True,
        check=False,
    )


def test_a_run_under_budget_passes_and_reports_its_peak() -> None:
    r = _run(1000, sys.executable, "-c", BIG)
    assert r.returncode == 0, r.stderr
    assert "peak_rss:" in r.stderr


def test_a_run_over_budget_fails_even_though_it_exited_zero() -> None:
    r = _run(100, sys.executable, "-c", BIG)
    assert r.returncode == 1
    assert "over the" in r.stderr


def test_a_failing_command_keeps_its_own_exit_status() -> None:
    r = _run(1000, sys.executable, "-c", "raise SystemExit(7)")
    assert r.returncode == 7


def test_a_grandchild_counts() -> None:
    """The docs build is `uv` -> `mem-guard` -> `zensical`: the peak is read
    from a descendant, not the direct child."""
    inner = (
        f"import subprocess,sys; subprocess.run([sys.executable,'-c',{BIG!r}])"
    )
    r = _run(100, sys.executable, "-c", inner)
    assert r.returncode == 1, r.stderr
