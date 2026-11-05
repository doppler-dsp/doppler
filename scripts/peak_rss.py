#!/usr/bin/env python3
"""Run a command and fail if any process it started peaked above a budget.

``mem-guard.sh`` puts a ceiling on a command with ``systemd-run``; on a box
with no systemd user session (every GitHub runner) it says so and runs the
command UNGUARDED, so a ceiling alone gates nothing there. This wrapper is
the check that works everywhere: it waits for the command and reads
``getrusage(RUSAGE_CHILDREN).ru_maxrss``, which is the largest resident set of
any descendant that was waited for, whoever launched it. A build that grows
past the budget fails even when it finishes, which is the point: the failure
this exists for (doppler#2840) was a docs build that ran away to 7 GB, and on
a big runner it would simply have succeeded, slowly, at 8 GB.

Usage
-----
    python scripts/peak_rss.py --max-mb 1536 -- <command> [args...]

The command's own exit status is returned when it fails; a pass that went over
budget returns 1. Where there is no ``resource`` module (Windows) it refuses
with status 2 and says why, rather than failing on the import. One line on
stderr reports the peak either way.
"""

from __future__ import annotations

import argparse
import subprocess
import sys

try:
    import resource
except ImportError:  # Windows: no getrusage, so no peak to read
    resource = None  # type: ignore[assignment]


def peak_mb() -> float:
    """Largest resident set (MB) of any waited-for descendant of this process.

    ``ru_maxrss`` is kilobytes on Linux and BYTES on macOS.
    """
    raw = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
    return raw / (1024 * 1024) if sys.platform == "darwin" else raw / 1024


def main(argv: list[str] | None = None) -> int:
    if resource is None:
        print(
            "peak_rss: peak RSS needs POSIX getrusage; the docs build runs "
            "on Linux and macOS",
            file=sys.stderr,
        )
        return 2
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--max-mb", type=float, required=True)
    ap.add_argument("cmd", nargs=argparse.REMAINDER)
    a = ap.parse_args(argv)
    cmd = a.cmd[1:] if a.cmd[:1] == ["--"] else a.cmd
    if not cmd:
        ap.error("no command after --")
    rc = subprocess.run(cmd, check=False).returncode
    peak = peak_mb()
    print(
        f"peak_rss: {peak:.0f} MB (budget {a.max_mb:.0f} MB)",
        file=sys.stderr,
    )
    if rc != 0:
        return rc
    if peak > a.max_mb:
        print(
            f"peak_rss: FAILED -- {cmd[0]} peaked at {peak:.0f} MB, over the "
            f"{a.max_mb:.0f} MB budget (a returned memory runaway, "
            "doppler#2840)",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
