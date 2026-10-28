#!/usr/bin/env python3
"""Gate: every built C benchmark runs to a clean exit.

``check_bench_coverage.py`` holds what is checkable WITHOUT running a
benchmark: it builds, it records a measurement, and it writes its JSON
under a name the collector opens. This gate holds the remaining half:
the benchmark runs, all the way to exit 0.

That half went unchecked until v0.77.0's release bench run. Two benchmarks
had broken a day earlier, both under deliberate behavior changes in the
same fix train (doppler#2659, #2675):

- ``bench_Resampler_core`` passed rate 0 to ``dp_Resampler_create``, which
  had started refusing it, then dereferenced the NULL (SIGSEGV).
- ``bench_ber_meter_core`` scored before aligning, after ``score()`` started
  refusing to count without an alignment, and its own no-op guard exited 1.

Each library change had green CI, because no job ever ran a benchmark: the
release run was the first place either could be seen, and the most
expensive.

Discovery, not registration: every executable named ``bench_*_core`` under
the build directory is run, so a new benchmark is gated the moment it
builds. Each runs in its own scratch directory, because benchmarks write
their JSON into the working directory. Finding none is a failure, not a
pass: a gate that ran nothing has checked nothing.

Usage: ``check_bench_runs.py BUILD_DIR [--timeout SECONDS]``
"""

from __future__ import annotations

import argparse
import signal
import subprocess
import sys
import tempfile
from pathlib import Path


def find_benches(build_dir: Path) -> list[Path]:
    """Every executable ``bench_*_core`` file under ``build_dir``, sorted."""
    return sorted(
        p
        for p in build_dir.rglob("bench_*_core")
        if p.is_file() and p.stat().st_mode & 0o111
    )


def describe(rc: int) -> str:
    """A return code as a reader wants it: a signal name, or the exit."""
    if rc < 0:
        try:
            return f"signal {signal.Signals(-rc).name}"
        except ValueError:
            return f"signal {-rc}"
    return f"exit {rc}"


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("build_dir", type=Path)
    ap.add_argument("--timeout", type=float, default=300.0)
    args = ap.parse_args(argv)

    benches = find_benches(args.build_dir)
    if not benches:
        print(
            f"bench-run-check: no bench_*_core executables under "
            f"{args.build_dir} -- build first; a run of nothing is not a "
            "pass",
            file=sys.stderr,
        )
        return 1

    failed: list[str] = []
    for exe in benches:
        with tempfile.TemporaryDirectory() as cwd:
            try:
                r = subprocess.run(
                    [str(exe.resolve())],
                    cwd=cwd,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.PIPE,
                    text=True,
                    errors="replace",
                    timeout=args.timeout,
                    check=False,
                )
            except subprocess.TimeoutExpired:
                failed.append(f"{exe.name}: timed out after {args.timeout}s")
                continue
        if r.returncode != 0:
            tail = r.stderr.strip().splitlines()[-3:]
            why = "; ".join(tail) if tail else "(nothing on stderr)"
            failed.append(f"{exe.name}: {describe(r.returncode)} -- {why}")

    if failed:
        print(
            f"bench-run-check: {len(failed)} of {len(benches)} benchmark(s) "
            "did not run cleanly:",
            file=sys.stderr,
        )
        for line in failed:
            print(f"  {line}", file=sys.stderr)
        return 1
    print(f"bench-run-check: all {len(benches)} benchmarks ran cleanly")
    return 0


if __name__ == "__main__":
    sys.exit(main())
