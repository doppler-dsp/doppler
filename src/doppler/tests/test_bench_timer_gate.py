"""The bench-timer gate, exercised over a seeded tree.

`scripts/check_bench_timer.py` exists because 85 of doppler's 106 benchmarks
carried their own copy of the same four-line `elapsed_sec()` and opened their
own `clock_gettime(CLOCK_MONOTONIC)`. They had not drifted, and nothing
stopped them -- the duplication only surfaced because the UCRT has neither
call, so every benchmark failed to compile on Windows. jm 0.76.4 (gh-1341)
put the clock in the vendored `jm_bench.h`; the gate is what keeps it there.

The cases below are about the gate's own failure modes, not about timing.
Each seeds a fake tree and points `--root` at it, because a gate that can
only run against the real tree cannot be sabotaged: you would have to break
all 106 benchmarks to check it, and nobody does that twice.

Two of them guard carve-outs that a blunter regex would get wrong, and both
are load-bearing. `jm_bench.h` is the sanctioned home, so the very spelling
the gate demands lives there and must not trip it. And `bench_stream.c`
stamps wall-clock times onto NATS messages with `CLOCK_REALTIME` and sleeps
interval `nanosleep`s -- a different clock for a different job, in a file
that is POSIX-only by its own CMake guard -- so banning `struct timespec`
outright would have forced an allowlist entry for a file that is not a
duplicate of anything.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_bench_timer.py"

BENCH = "native/benchmarks"

CLEAN = """\
int main (void) {
  uint64_t t0 = jm_bench_now_ns ();
  uint64_t t1 = jm_bench_now_ns ();
  double s = jm_bench_elapsed_sec (t0, t1);
  return (int)s;
}
"""

PRIVATE_CLOCK = """\
int main (void) {
  struct timespec t0;
  clock_gettime (CLOCK_MONOTONIC, &t0);
  return 0;
}
"""

PRIVATE_HELPER = """\
static double
elapsed_sec (uint64_t a, uint64_t b) { return (double)(b - a) * 1e-9; }
"""

# What bench_stream.c really does: a wall clock, not the bench timer.
WALL_CLOCK = """\
static uint64_t now_ns (void) {
  struct timespec ts;
  clock_gettime (CLOCK_REALTIME, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
"""


def _seed(tmp_path: Path, files: dict[str, str]) -> Path:
    """A minimal tree shaped like native/benchmarks."""
    for rel, body in files.items():
        p = tmp_path / BENCH / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(body, encoding="utf-8")
    return tmp_path


def _run(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root)],
        capture_output=True,
        text=True,
    )


def test_a_converted_tree_passes(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path, {"bench_a_core.c": CLEAN}))
    assert r.returncode == 0, r.stdout + r.stderr
    assert "jm_bench_now_ns" in r.stdout


def test_a_hand_opened_monotonic_clock_fails(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path, {"bench_a_core.c": PRIVATE_CLOCK}))
    assert r.returncode == 1, r.stdout + r.stderr
    assert "bench_a_core.c" in r.stdout


def test_a_regrown_private_helper_fails(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path, {"bench_a_core.c": PRIVATE_HELPER}))
    assert r.returncode == 1, r.stdout + r.stderr
    assert "elapsed_sec" in r.stdout


def test_the_sanctioned_spelling_is_not_a_false_positive(
    tmp_path: Path,
) -> None:
    """`jm_bench_elapsed_sec` ends in the banned name -- hence the
    lookbehind."""
    r = _run(_seed(tmp_path, {"bench_a_core.c": CLEAN}))
    assert r.returncode == 0, r.stdout + r.stderr


def test_the_wall_clock_carve_out_passes(tmp_path: Path) -> None:
    """bench_stream.c's CLOCK_REALTIME is a different clock, not a copy."""
    r = _run(_seed(tmp_path, {"bench_stream.c": WALL_CLOCK}))
    assert r.returncode == 0, r.stdout + r.stderr


def test_jm_bench_h_may_hold_the_implementation(tmp_path: Path) -> None:
    """The sanctioned home contains the very call the gate bans elsewhere."""
    r = _run(_seed(tmp_path, {"jm_bench.h": PRIVATE_CLOCK}))
    assert r.returncode == 0, r.stdout + r.stderr


def test_prose_about_the_clock_is_not_a_call(tmp_path: Path) -> None:
    doc = (
        "/* Every benchmark used to call clock_gettime(CLOCK_MONOTONIC). */\n"
    )
    r = _run(_seed(tmp_path, {"bench_a_core.c": doc + CLEAN}))
    assert r.returncode == 0, r.stdout + r.stderr


def test_one_offender_among_many_is_still_found(tmp_path: Path) -> None:
    """The scan does not stop at the first clean file."""
    files = {f"bench_{n}_core.c": CLEAN for n in "abcdef"}
    files["bench_z_core.c"] = PRIVATE_CLOCK
    r = _run(_seed(tmp_path, files))
    assert r.returncode == 1, r.stdout + r.stderr
    assert "bench_z_core.c" in r.stdout
