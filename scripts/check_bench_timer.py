#!/usr/bin/env python3
"""Gate: a benchmark times and records through jm_bench.h's primitives.

Two rules, one scan of `native/benchmarks`: the clock is `jm_bench_now_ns()`
(below), and every row names its unit through `jm_bench_add_unit()` (at the
end).

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

A row names its unit (#2314). A row is one ROUND of `iters` units of work,
and the report prints its time per unit -- so it must know whether a unit is
a call, a sample, a frame. Before jm 0.106.0 a row recorded no unit at all,
and the page printed every C row's round as one call: 545 of v0.71.0's 566
published C rows read a whole round as "time/call", as they had in every
release from v0.43.0. jm 0.106.0's `jm_bench_add()` still does not ask: it
records every row as calls, which would label `acc_f32::steps`, one call
over 65,536 samples, the same way as `acc_f32::step`, 65,536 calls of one
sample each. So a bare `jm_bench_add()` is banned: every
row goes through `jm_bench_add_unit()` (or doppler's `dp_bench_record()`,
which forwards its unit), and the author says what one of `iters` is. The
gate cannot tell whether the noun is RIGHT; it only stops the silent
default. Whether a unit is well formed is jm's rule, checked by jm when the
row is recorded -- not restated here, where it would drift from jm's.

Usage:  python3 scripts/check_bench_timer.py
Exit 0 when every benchmark times and records through jm_bench.h.
"""

from __future__ import annotations

import argparse
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
# A row recorded without its unit. `jm_bench_add_unit` shares the prefix;
# the `\s*\(` right after the name is what tells them apart.
UNITLESS = re.compile(r"(?<![\w])jm_bench_add\s*\(")


def offenders(root: Path = ROOT) -> list[tuple[str, int, str]]:
    found: list[tuple[str, int, str]] = []
    for path in sorted((root / SCAN_DIR).rglob("*")):
        if path.suffix not in (".c", ".h"):
            continue
        rel = path.relative_to(root).as_posix()
        if rel == SANCTIONED:
            continue
        for i, line in enumerate(
            path.read_text(errors="replace").splitlines()
        ):
            if line.lstrip().startswith(("*", "/*", "//")):
                continue  # prose about the clock is not a call
            if (
                MONOTONIC.search(line)
                or HELPER.search(line)
                or UNITLESS.search(line)
            ):
                found.append((rel, i + 1, line.strip()))
    return found


def main() -> int:
    # --root exists for this gate's OWN test. A gate that can only run
    # against the real tree cannot be sabotaged -- you would have to break
    # doppler's 106 benchmarks to check it, and nobody does that twice --
    # so the test seeds a tree shaped like native/benchmarks and points the
    # scan at that instead.
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT)
    found = offenders(ap.parse_args().root)
    if not found:
        print(
            "bench-timer: every benchmark times through jm_bench_now_ns()"
            " and names each row's unit"
        )
        return 0
    timers = [f for f in found if not UNITLESS.search(f[2])]
    rows = [f for f in found if UNITLESS.search(f[2])]
    if timers:
        print("bench-timer: a private benchmark timer is opened here --")
        for rel, n, line in timers:
            print(f"  {rel}:{n}: {line}")
        print(
            "  Use jm_bench_now_ns() / jm_bench_elapsed_sec(t0, t1) from\n"
            "  native/benchmarks/jm_bench.h instead. They are already\n"
            "  included: the clock lives there once so it can be ported\n"
            "  once -- the UCRT has no clock_gettime, and a private copy is\n"
            "  a benchmark that cannot build on Windows and drifts from the\n"
            "  other 105."
        )
    if rows:
        print("bench-timer: a row is recorded without its unit --")
        for rel, n, line in rows:
            print(f"  {rel}:{n}: {line}")
        print(
            "  Use jm_bench_add_unit(..., unit), where unit is what ONE of\n"
            "  `iters` is: JM_BENCH_SAMPLE for one call over `iters`\n"
            "  samples or a loop of `iters` one-sample step() calls (as\n"
            "  jm's scaffold labels it), JM_BENCH_CALL for a loop of\n"
            "  calls that are not one sample each (a getter, a setter),\n"
            '  JM_BENCH_FRAME, or the counted noun ("bit", "codeword").\n'
            "  jm_bench_add() records every row as calls, and the report\n"
            "  prints time per unit from it (#2314)."
        )
    return 1


if __name__ == "__main__":
    sys.exit(main())
