#!/usr/bin/env python3
"""A/B two commits on the benchmark suite: is B at least as fast as A?

`make bench-interleaved` answers a different question -- portable against
native builds of ONE commit, merged into a published set -- so every PR
that had to show it did not slow a row wrote a scratch harness for it
(#2744's is the one this replaces). This is that harness, once (#2759):

1. check A and B out into two throwaway worktrees and build both at the
   same flags (``--build portable`` by default, or ``native``), with
   ``bench_interleaved.setup_worktree`` -- the release tool's own builder;
2. run the suite alternately K times, A then B on even passes and B then A
   on odd ones, so neither side is systematically first, pinned to the
   fastest core class (``bench_report.fastest_cpus``) as a release run is;
3. per row, keep each pass's best round (``stats.min``), then report for
   each side the min of those K values and its spread (max - min over K),
   and the ratio B / A;
4. call a row a TIE when the two mins differ by no more than the larger of
   the two spreads -- the run's own noise, measured, not a threshold picked
   in advance -- SLOWER when B's min exceeds A's by more than that, and
   FASTER when it is below by more.

It exits 1 when any row is SLOWER, or when a row A measured is missing from
B (a gate cannot pass on a row it never saw), and writes nothing under
``benchmarks/published/``. ``BENCH_ARGS`` narrows the suite exactly as it
does for ``make bench``, e.g. ``BENCH_ARGS="--c-only acq"``.

Examples
--------
    make bench-ab A=origin/main B=HEAD                     # whole suite, K=5
    make bench-ab A=origin/main B=HEAD K=7 \\
        BENCH_ARGS="--c-only async_dsss_pool"              # one C component
    make bench-ab A=v0.78.0 B=HEAD BUILD=native JSON=ab.json
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench_interleaved import (
    BUILD_ARGS,
    REPO,
    _remove_worktrees,
    bench_once,
    setup_worktree,
)
from bench_report import fastest_cpus, machine_not_ready

#: The two worktrees, apart from bench-interleaved's so the tools never
#: share a checkout.
WORKTREES = {"A": "/tmp/doppler-bench-ab-A", "B": "/tmp/doppler-bench-ab-B"}


def best_rounds(snaps: list) -> dict:
    """{row: [best round of each pass]} over K snapshots of one suite.

    A row is named by its ``fullname`` (the bench binary or test module
    and the row), falling back to ``name``. A pass whose row has no
    positive ``stats.min`` contributes nothing for that row.

    >>> s = [
    ...     {"benchmarks": [{"fullname": "x", "stats": {"min": 2.0}}]},
    ...     {
    ...         "benchmarks": [
    ...             {"fullname": "x", "stats": {"min": 1.5}},
    ...             {"name": "y", "stats": {"min": 0}},
    ...         ]
    ...     },
    ... ]
    >>> best_rounds(s)
    {'x': [2.0, 1.5]}
    """
    out: dict = {}
    for s in snaps:
        for b in s.get("benchmarks", []):
            key = b.get("fullname") or b.get("name")
            m = b.get("stats", {}).get("min")
            if key and isinstance(m, (int, float)) and m > 0:
                out.setdefault(key, []).append(float(m))
    return out


def verdict(a: list, b: list) -> dict:
    """Compare one row's K best rounds on each side.

    TIE when the two mins differ by no more than the larger spread, the
    noise this very run measured; otherwise SLOWER or FASTER for B.

    >>> v = verdict([1.00, 1.02, 1.01], [1.01, 1.03, 1.00])
    >>> v["verdict"], round(v["ratio"], 3)
    ('TIE', 1.0)
    >>> verdict([1.00, 1.01], [1.20, 1.21])["verdict"]
    'SLOWER'
    >>> verdict([1.20, 1.21], [1.00, 1.01])["verdict"]
    'FASTER'
    >>> verdict([1.0], [1.0])["spread"]  # one pass has no spread: exact
    0.0
    """
    ma, mb = min(a), min(b)
    sa, sb = max(a) - ma, max(b) - mb
    spread = max(sa, sb)
    if mb - ma > spread:
        v = "SLOWER"
    elif ma - mb > spread:
        v = "FASTER"
    else:
        v = "TIE"
    return {
        "a": ma,
        "b": mb,
        "spread_a": sa,
        "spread_b": sb,
        "spread": spread,
        "ratio": mb / ma,
        "verdict": v,
    }


def compare(a: dict, b: dict) -> dict:
    """{row: verdict} for every row either side measured.

    A row only A has is MISSING (B dropped it, or it failed to record); a
    row only B has is NEW. Neither has a ratio.

    >>> r = compare({"x": [1.0], "gone": [1.0]}, {"x": [1.0], "new": [2.0]})
    >>> sorted((k, v["verdict"]) for k, v in r.items())
    [('gone', 'MISSING'), ('new', 'NEW'), ('x', 'TIE')]
    """
    out = {}
    for row in sorted(set(a) | set(b)):
        if row not in b:
            out[row] = {"verdict": "MISSING"}
        elif row not in a:
            out[row] = {"verdict": "NEW"}
        else:
            out[row] = verdict(a[row], b[row])
    return out


def failures(result: dict) -> list:
    """The rows that fail the gate: SLOWER or MISSING, in row order.

    >>> failures(
    ...     {
    ...         "x": {"verdict": "TIE"},
    ...         "y": {"verdict": "SLOWER"},
    ...         "z": {"verdict": "MISSING"},
    ...         "w": {"verdict": "NEW"},
    ...     }
    ... )
    ['y', 'z']
    """
    return [
        r for r, v in result.items() if v["verdict"] in ("SLOWER", "MISSING")
    ]


def render(result: dict) -> str:
    """A fixed-width table, one row per benchmark, times in microseconds.

    >>> table = render({"x": verdict([1e-6], [2e-6])})
    >>> print(table)  # doctest: +NORMALIZE_WHITESPACE
    row  A us  B us  B/A  spread us  verdict
    x    1.000 2.000 2.000 0.000     SLOWER
    """
    head = ("row", "A us", "B us", "B/A", "spread us", "verdict")
    lines = []
    for row, v in result.items():
        if "ratio" in v:
            lines.append(
                (
                    row,
                    f"{v['a'] * 1e6:.3f}",
                    f"{v['b'] * 1e6:.3f}",
                    f"{v['ratio']:.3f}",
                    f"{v['spread'] * 1e6:.3f}",
                    v["verdict"],
                )
            )
        else:
            lines.append((row, "", "", "", "", v["verdict"]))
    widths = [max(len(r[i]) for r in [head, *lines]) for i in range(6)]
    fmt = "  ".join(f"{{:<{w}}}" for w in widths)
    return "\n".join(fmt.format(*r).rstrip() for r in [head, *lines])


def _resolve(ref: str) -> str:
    r = subprocess.run(
        ["git", "rev-parse", "--short", "--verify", f"{ref}^{{commit}}"],
        cwd=REPO,
        text=True,
        capture_output=True,
    )
    if r.returncode != 0:
        raise SystemExit(f"bench-ab: {ref!r} names no commit")
    return r.stdout.strip()


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("a", help="the baseline ref")
    ap.add_argument("b", help="the candidate ref")
    ap.add_argument("-k", type=int, default=5, help="passes per side")
    ap.add_argument(
        "--build",
        choices=sorted(BUILD_ARGS),
        default="portable",
        help="the flags BOTH sides build at",
    )
    ap.add_argument("--json", help="also write the result here")
    args = ap.parse_args(argv)
    if args.k < 2:
        print("bench-ab: K must be at least 2, or no row has a spread")
        return 2

    commits = {"A": _resolve(args.a), "B": _resolve(args.b)}
    print(
        f"A = {args.a} ({commits['A']}), B = {args.b} ({commits['B']}), "
        f"K = {args.k}, {args.build} build",
        flush=True,
    )
    # Both sides run under the same conditions, interleaved, so drift
    # cancels; an unready machine widens the spread rather than biasing
    # one side. Say so instead of refusing.
    for p in machine_not_ready():
        print(f"  warning: {p} (the spreads will be wider)")

    wts = {}
    try:
        for side in ("A", "B"):
            wts[side] = setup_worktree(
                args.build,
                commits[side],
                wt=WORKTREES[side],
                cmake_args=BUILD_ARGS[args.build],
            )
        cpus = fastest_cpus()
        samples = {s: {"py": [], "c": []} for s in ("A", "B")}
        for i in range(args.k):
            for side in ("A", "B") if i % 2 == 0 else ("B", "A"):
                print(f"pass {i + 1}/{args.k} [{side}] ...", flush=True)
                py, c = bench_once(wts[side], cpus)
                samples[side]["py"].append(py)
                samples[side]["c"].append(c)
    finally:
        _remove_worktrees(wts)

    result = {}
    for suite in ("c", "py"):
        part = compare(
            best_rounds(samples["A"][suite]), best_rounds(samples["B"][suite])
        )
        result.update({f"{suite}:{row}": v for row, v in part.items()})
    if not any("ratio" in v for v in result.values()):
        print("bench-ab: no row was measured on both sides")
        return 1
    print(render(result))
    if args.json:
        with open(args.json, "w") as fh:
            json.dump(
                {
                    "a": commits["A"],
                    "b": commits["B"],
                    "k": args.k,
                    "build": args.build,
                    "rows": result,
                },
                fh,
                indent=1,
            )
            fh.write("\n")
    bad = failures(result)
    counts = {
        v: sum(r["verdict"] == v for r in result.values())
        for v in ("FASTER", "TIE", "SLOWER", "MISSING", "NEW")
    }
    print(", ".join(f"{n} {v.lower()}" for v, n in counts.items() if n))
    if bad:
        print(f"bench-ab: B fails on {len(bad)} row(s): {', '.join(bad)}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
