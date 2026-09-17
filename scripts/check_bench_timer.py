#!/usr/bin/env python3
"""Gate: a benchmark reads the monotonic clock through `jm_bench_now_ns()`.

Every generated benchmark used to open its own `clock_gettime(CLOCK_MONOTONIC)`
and carry its own `elapsed_sec()` helper -- 85 private copies of one
four-line primitive, which is the shape this repo has been burned by five
times already (`dp_fmod_pos`, `dp_lgamma`, `dp_format_full_scale`, the
phase-conversion trio, the alloc helpers). They had not drifted *yet*, but
nothing stopped them: a fix to the rounding in one copy would have left 84
wrong, and the only reason that had not happened is that nobody had needed
to touch it.

Windows is what forced the issue. The UCRT has neither `clock_gettime` nor
`struct timespec` as the benchmarks used them, so all 106 failed to compile
under MSVC. The fix belongs upstream, not here: jm 0.76.4 (gh-1341) puts the
clock in `jm_bench.h` -- `jm_bench_now_ns()` over QPC on Windows and
`clock_gettime(CLOCK_MONOTONIC)` elsewhere, plus `jm_bench_elapsed_sec()` --
and that header is vendored into every benchmark already, so it is the one
place that fixes all of them.

`CLOCK_MONOTONIC` in `native/benchmarks` is therefore a private timer by
definition, and a private `elapsed_sec` is the helper coming back. Neither
needs an allowlist: every one was converted when this gate landed, so the
first new one fails.

NOT banned, deliberately: `CLOCK_REALTIME` and `struct timespec`.
`bench_stream.c` stamps wall-clock times onto NATS messages and sleeps
interval `nanosleep`s -- a different clock for a different job, and that
benchmark is POSIX-only by its own `if(NOT WIN32)` CMake guard, so it is
neither a duplicate of the bench timer nor a Windows problem.

Usage:  python3 scripts/check_bench_timer.py
Exit 0 when every benchmark times through jm_bench.h.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCAN_DIR = "native/benchmarks"
# jm_bench.h IS the implementation -- it is vendored and create-only, so it
# is also the one file a local edit must never "fix" (doppler has lost two
# private edits to a re-vendor that way).
SANCTIONED = "native/benchmarks/jm_bench.h"

# The bench timer, wherever it is opened by hand.
MONOTONIC = re.compile(r"(?<![\w])CLOCK_MONOTONIC(?![\w])")
# The helper coming back. `jm_bench_elapsed_sec` is the sanctioned one and
# ends in the same characters, so the lookbehind is load-bearing.
HELPER = re.compile(r"(?<![\w])elapsed_sec(?![\w])")


def offenders() -> list[tuple[str, int, str]]:
    found: list[tuple[str, int, str]] = []
    for path in sorted((ROOT / SCAN_DIR).rglob("*")):
        if path.suffix not in (".c", ".h"):
            continue
        rel = path.relative_to(ROOT).as_posix()
        if rel == SANCTIONED:
            continue
        for i, line in enumerate(
            path.read_text(errors="replace").splitlines()
        ):
            if line.lstrip().startswith(("*", "/*", "//")):
                continue  # prose about the clock is not a call
            if MONOTONIC.search(line) or HELPER.search(line):
                found.append((rel, i + 1, line.strip()))
    return found


def main() -> int:
    found = offenders()
    if not found:
        print("bench-timer: every benchmark times through jm_bench_now_ns()")
        return 0
    print("bench-timer: a private benchmark timer is opened here --")
    for rel, n, line in found:
        print(f"  {rel}:{n}: {line}")
    print(
        "  Use jm_bench_now_ns() / jm_bench_elapsed_sec(t0, t1) from\n"
        "  native/benchmarks/jm_bench.h instead. They are already included:\n"
        "  the clock lives there once so it can be ported once -- the UCRT\n"
        "  has no clock_gettime, and a private copy is a benchmark that\n"
        "  cannot build on Windows and drifts from the other 105."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
