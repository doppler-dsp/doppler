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
   each side the min of those K values, its band (second-best minus best
   pass), and the ratio B / A;
4. call a row a TIE when the two mins differ by no more than the
   tolerance: the larger of the two bands -- the run's own noise on the
   min -- and never less than 1% of A's min (``FLOOR``, the bias a null
   A/A run of one commit measured); SLOWER when B's min exceeds A's by more
   than that, and FASTER when it is below by more. The band ignores a single
   slow pass on purpose: the full max - min would let one outlier hide a
   regression (#2771).

It exits 1 when any row is SLOWER, or when a row A measured is missing from
B (a gate cannot pass on a row it never saw), and writes nothing under
``benchmarks/published/``. ``BENCH_ARGS`` narrows the suite exactly as it
does for ``make bench``, e.g. ``BENCH_ARGS="--c-only acq"``.

``ALIAS=old=new`` judges a family renamed between the two commits: A's
``old::<row>`` against B's ``new::<row>``, suffix for suffix, so a rename
keeps every row comparable. Rows are keyed by their ``fullname``, whose
family for a C row is the BENCH BINARY, ``bench_corr2d_core``, not the
published name's ``corr2d``. So #2752 step 3.2's rename is
``ALIAS=bench_corr2d_core=bench_correlator_core``. It is declared per run,
never defaulted, and printed with the verdict. Within an aliased family a
row on one side only is UNPAIRED and fails. An alias that pairs nothing
refuses the run, and is caught after the first pass of each side, not
after all of them.

Examples
--------
    make bench-ab A=origin/main B=HEAD                     # whole suite, K=5
    make bench-ab A=origin/main B=HEAD K=7 \\
        BENCH_ARGS="--c-only async_dsss_pool"              # one C component
    make bench-ab A=v0.78.0 B=HEAD BUILD=native JSON=ab.json
    make bench-ab A=origin/main B=HEAD \\
        ALIAS=bench_corr2d_core=bench_correlator_core      # a renamed bench
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench_interleaved import (
    BUILD_ARGS,
    REPO,
    _remove_worktrees,
    bench_once,
    setup_worktree,
)
from bench_report import fastest_cpus, machine_not_ready


def worktree_path(side: str) -> str:
    """A fresh path for one side's worktree, unique to this run.

    A fixed path let two concurrent runs remove each other's checkout
    (#2771). The directory is made and removed again so `git worktree add`
    can create it; the name stays this run's.
    """
    d = tempfile.mkdtemp(prefix=f"doppler-bench-ab-{side}-")
    os.rmdir(d)
    return d


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


#: The smallest difference, as a fraction of A's min, that is a verdict.
#: Below it two worktrees of the SAME commit already differ: a null A/A run
#: on the bench box (cachyos-x8664-ai465, K = 5, 2026-11-02) read every B
#: pass of two rows above A's best, 0.36% and 0.54% apart, a steady bias of
#: the checkout (its path, its environment) that no band over passes sees.
#: 1% is about twice that. A real slowdown under 1% reads as a TIE.
FLOOR = 0.01


def band(x: list) -> float:
    """One side's noise band: its second-best pass minus its best.

    The gate compares mins, so the noise that matters is how far the min
    itself moves from pass to pass, which the gap between the two best
    passes estimates. The full max - min over K does not: one slow pass (a
    cron job, a thermal step) widens it to whatever that pass cost, and a
    50% regression then hides inside it (#2771). At K = 2 the two are the
    same, so `main` asks for K >= 3. One pass has no band.

    >>> band([1.0, 1.0, 1.0, 1.0, 3.0])  # the outlier is ignored
    0.0
    >>> round(band([1.02, 1.00, 1.05]), 6)
    0.02
    >>> band([1.0])
    0.0
    """
    if len(x) < 2:
        return 0.0
    lo = sorted(x)
    return lo[1] - lo[0]


def verdict(a: list, b: list) -> dict:
    """Compare one row's K best rounds on each side.

    TIE when the two mins differ by no more than the tolerance: the larger
    of the two sides' bands (`band`: second-best minus best pass, the noise
    this very run measured on the min), and never less than `FLOOR` of A's
    min, the bias two checkouts of one commit already show. Otherwise
    SLOWER or FASTER for B. `spread_a` and `spread_b` keep each side's
    full max - min for the reader.

    >>> v = verdict([1.00, 1.02, 1.01], [1.01, 1.03, 1.00])
    >>> v["verdict"], round(v["ratio"], 3)
    ('TIE', 1.0)
    >>> verdict([1.00, 1.01], [1.20, 1.21])["verdict"]
    'SLOWER'
    >>> verdict([1.20, 1.21], [1.00, 1.01])["verdict"]
    'FASTER'
    >>> verdict([1, 1, 1, 1, 3], [1.5] * 5)["verdict"]  # one outlier pass
    'SLOWER'
    >>> verdict([1.0] * 5, [1.005] * 5)["verdict"]  # under the 1% floor
    'TIE'
    >>> verdict([1.0], [1.0])["band"]  # one pass has no band
    0.0
    """
    ma, mb = min(a), min(b)
    width = max(band(a), band(b), FLOOR * ma)
    if mb - ma > width:
        v = "SLOWER"
    elif ma - mb > width:
        v = "FASTER"
    else:
        v = "TIE"
    return {
        "a": ma,
        "b": mb,
        "band": max(band(a), band(b)),
        "tol": width,
        "spread_a": max(a) - ma,
        "spread_b": max(b) - mb,
        "passes_a": list(a),
        "passes_b": list(b),
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


def families(snaps: list) -> list:
    """The row families in some snapshots, sorted: what an alias can name.

    >>> row = {"fullname": "bench_x_core::r", "stats": {"min": 1.0}}
    >>> families([{"benchmarks": [row]}])
    ['bench_x_core']
    """
    return sorted(
        {k.split("::", 1)[0] for k in best_rounds(snaps) if "::" in k}
    )


def parse_aliases(spec: str) -> list:
    """`old=new` family renames, separated by commas or spaces.

    A family is a row's prefix before its first ``::``, in the key bench-ab
    compares, the ``fullname``: ``bench_corr2d_core`` in
    ``bench_corr2d_core::single_row_fast_path``. Only whole families are
    renamed, and a row's suffix must match exactly, so nothing pairs by
    resemblance.

    >>> parse_aliases("corr2d=correlator")
    [('corr2d', 'correlator')]
    >>> parse_aliases("a=b, c=d")
    [('a', 'b'), ('c', 'd')]
    >>> parse_aliases("corr2d")
    Traceback (most recent call last):
    ...
    ValueError: alias 'corr2d' is not old=new
    """
    out = []
    for item in spec.replace(",", " ").split():
        old, eq, new = item.partition("=")
        if not eq or not old or not new or "::" in old + new or old == new:
            raise ValueError(f"alias {item!r} is not old=new")
        out.append((old, new))
    return out


def apply_aliases(a: dict, b: dict, aliases: list) -> tuple:
    """A's rows renamed into B's families; which rows the aliases govern.

    Returns ``(a_renamed, governed, problems)``:

    - ``a_renamed`` is A with every ``old::suffix`` row keyed
      ``new::suffix``, so `compare` pairs it with B's row of that name;
    - ``governed`` maps each key an alias covers (every renamed A row, and
      every B row in a ``new`` family) to the A name it was measured under,
      or ``None`` for a B row A never had;
    - ``problems`` names what refuses the run before any verdict: an alias
      matching no row on one side (an alias that pairs nothing passes
      nothing), or a renamed row colliding with a row A already has.

    >>> a = {"corr2d::x": [1.0], "fft::y": [1.0]}
    >>> b = {"correlator::x": [1.0], "fft::y": [1.0]}
    >>> ar, gov, bad = apply_aliases(a, b, [("corr2d", "correlator")])
    >>> sorted(ar), gov, bad
    (['correlator::x', 'fft::y'], {'correlator::x': 'corr2d::x'}, [])
    """
    renamed = dict(a)
    governed: dict = {}
    problems = []
    for old, new in aliases:
        olds = [k for k in a if k.startswith(f"{old}::")]
        news = [k for k in b if k.startswith(f"{new}::")]
        if not olds:
            problems.append(f"alias {old}={new}: A has no {old}:: row")
        if not news:
            problems.append(f"alias {old}={new}: B has no {new}:: row")
        for k in olds:
            to = f"{new}::{k[len(old) + 2 :]}"
            if to in a:
                problems.append(
                    f"alias {old}={new}: {k} would replace A's own {to}"
                )
                continue
            renamed[to] = renamed.pop(k)
            governed[to] = k
        for k in news:
            governed.setdefault(k, None)
    return renamed, governed, problems


def pair_governed(result: dict, governed: dict) -> dict:
    """Refuse every aliased row that did not pair: absent is not a pass.

    Within a renamed family, a row on one side only would otherwise read as
    MISSING (A only) or, worse, NEW (B only), which the gate lets through.
    Either is UNPAIRED here, and UNPAIRED fails. A paired row keeps its
    verdict and records the A name it was measured under.

    >>> r = {"n::x": {"verdict": "TIE", "ratio": 1.0}}
    >>> r["n::y"] = {"verdict": "NEW"}
    >>> out = pair_governed(r, {"n::x": "o::x", "n::y": None})
    >>> out["n::x"]["alias_from"], out["n::y"]["verdict"]
    ('o::x', 'UNPAIRED')
    """
    out = dict(result)
    for key, was in governed.items():
        v = dict(out.get(key, {"verdict": "MISSING"}))
        if "ratio" in v:
            v["alias_from"] = was
        else:
            v = {"verdict": "UNPAIRED", "alias_from": was}
        out[key] = v
    return out


def failures(result: dict) -> list:
    """The rows that fail the gate: SLOWER, MISSING or UNPAIRED, in order.

    >>> failures(
    ...     {
    ...         "x": {"verdict": "TIE"},
    ...         "y": {"verdict": "SLOWER"},
    ...         "z": {"verdict": "MISSING"},
    ...         "w": {"verdict": "NEW"},
    ...         "u": {"verdict": "UNPAIRED"},
    ...     }
    ... )
    ['y', 'z', 'u']
    """
    return [
        r
        for r, v in result.items()
        if v["verdict"] in ("SLOWER", "MISSING", "UNPAIRED")
    ]


def judge(samples: dict, aliases: list) -> tuple:
    """Both suites' verdicts, with the aliases applied: (result, problems).

    The aliases are applied to each suite's rows; an alias is refused only
    when it pairs nothing in EITHER suite (a C family has no Python rows),
    so `problems` keeps the per-alias complaints that hold in both.

    >>> c = {"benchmarks": [{"fullname": "old::x", "stats": {"min": 1.0}}]}
    >>> d = {"benchmarks": [{"fullname": "new::x", "stats": {"min": 1.0}}]}
    >>> s = {"A": {"c": [c] * 3, "py": []}, "B": {"c": [d] * 3, "py": []}}
    >>> r, bad = judge(s, [("old", "new")])
    >>> r["c:new::x"]["verdict"], r["c:new::x"]["alias_from"], bad
    ('TIE', 'old::x', [])
    """
    result: dict = {}
    seen: dict = {}
    for suite in ("c", "py"):
        a = best_rounds(samples["A"][suite])
        b = best_rounds(samples["B"][suite])
        a, governed, bad = apply_aliases(a, b, aliases)
        for p in bad:
            seen.setdefault(p, set()).add(suite)
        part = pair_governed(compare(a, b), governed)
        result.update({f"{suite}:{row}": v for row, v in part.items()})
    # A "has no row" complaint counts only when it holds in both suites; a
    # collision is a problem wherever it happens.
    problems = [p for p, s in seen.items() if "has no" not in p or len(s) == 2]
    return result, problems


def render(result: dict) -> str:
    """A fixed-width table, one row per benchmark, times in microseconds.

    >>> table = render({"x": verdict([1e-6], [2e-6])})
    >>> print(table)  # doctest: +NORMALIZE_WHITESPACE
    row  A us  B us  B/A  tol us  verdict
    x    1.000 2.000 2.000 0.010   SLOWER
    """
    head = ("row", "A us", "B us", "B/A", "tol us", "verdict")
    lines = []
    for key, v in result.items():
        # An aliased row names what A measured it as, so the pairing is in
        # the verdict table and not only in the alias line above it.
        row = f"{key} <- {v['alias_from']}" if v.get("alias_from") else key
        if "ratio" in v:
            lines.append(
                (
                    row,
                    f"{v['a'] * 1e6:.3f}",
                    f"{v['b'] * 1e6:.3f}",
                    f"{v['ratio']:.3f}",
                    f"{v['tol'] * 1e6:.3f}",
                    v["verdict"],
                )
            )
        else:
            lines.append((row, "", "", "", "", v["verdict"]))
    widths = [max(len(r[i]) for r in [head, *lines]) for i in range(6)]
    fmt = "  ".join(f"{{:<{w}}}" for w in widths)
    return "\n".join(fmt.format(*r).rstrip() for r in [head, *lines])


def dirty_head(ref: str, commit: str) -> str:
    """Why `ref` cannot be measured as named, or "" if it can.

    A worktree is checked out from the COMMIT, so `B=HEAD` with uncommitted
    changes measures the committed tree and silently drops the edit
    (#2771). Refuse only a ref SPELLED as the checkout (``HEAD`` or ``@``)
    whose tracked files differ from it. A branch name or a sha names a
    commit, and means it even when it is HEAD's: `A=main` on a dirty main
    is a fair question. Untracked files are not part of a build.

    >>> dirty_head("main", "abc1234")  # a name, not the checkout: no git
    ''
    """
    if ref not in ("HEAD", "@"):
        return ""
    head = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"],
        cwd=REPO,
        text=True,
        capture_output=True,
    ).stdout.strip()
    if commit != head:
        return ""
    dirty = subprocess.run(
        ["git", "status", "--porcelain", "--untracked-files=no"],
        cwd=REPO,
        text=True,
        capture_output=True,
    ).stdout.strip()
    if not dirty:
        return ""
    return (
        f"{ref} is the checkout's HEAD ({commit}) and its tracked files "
        "have uncommitted changes, which a worktree would not see: commit "
        "them, or name the commit you mean"
    )


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
    ap.add_argument(
        "--alias",
        default="",
        help="old=new family renames, pairing A's old::row with B's new::row",
    )
    args = ap.parse_args(argv)
    try:
        aliases = parse_aliases(args.alias)
    except ValueError as e:
        print(f"bench-ab: {e}")
        return 2
    if args.k < 3:
        # The band is second-best minus best. At K = 2 that is max - min,
        # which one slow pass widens enough to hide a regression (#2771).
        print(
            "bench-ab: K must be at least 3, or one outlier pass sets the band"
        )
        return 2

    commits = {"A": _resolve(args.a), "B": _resolve(args.b)}
    for ref in (args.a, args.b):
        why = dirty_head(ref, _resolve(ref))
        if why:
            print(f"bench-ab: {why}")
            return 2
    print(
        f"A = {args.a} ({commits['A']}), B = {args.b} ({commits['B']}), "
        f"K = {args.k}, {args.build} build",
        flush=True,
    )
    # Both sides run under the same conditions, interleaved, so drift
    # cancels; an unready machine widens the bands rather than biasing
    # one side. Say so instead of refusing.
    for p in machine_not_ready():
        print(f"  warning: {p} (the bands will be wider)")

    wts = {}
    try:
        for side in ("A", "B"):
            wts[side] = setup_worktree(
                args.build,
                commits[side],
                wt=worktree_path(side),
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
            # An alias that pairs nothing is known once each side has run
            # once: stop then, not after every pass (#2752 step 3.2 lost a
            # full K=5 run to a mis-keyed alias).
            if i == 0 and aliases:
                early = judge(samples, aliases)[1]
                if early:
                    for p in early:
                        print(f"bench-ab: {p}")
                    for side in ("A", "B"):
                        fams = families(samples[side]["c"])
                        print(f"  {side}'s C families: {', '.join(fams)}")
                    return 1
    finally:
        _remove_worktrees(wts)

    result, problems = judge(samples, aliases)
    for a_, b_ in aliases:
        print(f"alias in force: A {a_}::<row> is B {b_}::<row>")
    if problems:
        for p in problems:
            print(f"bench-ab: {p}")
        return 1
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
                    "aliases": [f"{o}={n}" for o, n in aliases],
                    "rows": result,
                },
                fh,
                indent=1,
            )
            fh.write("\n")
    bad = failures(result)
    counts = {
        v: sum(r["verdict"] == v for r in result.values())
        for v in ("FASTER", "TIE", "SLOWER", "MISSING", "UNPAIRED", "NEW")
    }
    print(", ".join(f"{n} {v.lower()}" for v, n in counts.items() if n))
    if bad:
        print(f"bench-ab: B fails on {len(bad)} row(s): {', '.join(bad)}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
