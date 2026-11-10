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

**A SLOWER row is confirmed by a second run before it stops anything**
(#2984). An A/A run of one commit on a pinned bench box read 11 of 884 rows
SLOWER and 16 FASTER: about 1% of rows cross the tolerance by chance, so a
whole-suite run is rarely clean even on identical code. Rerun with
``--confirm FIRST.json`` (``make bench-ab ... CONFIRM=first.json``), the
first run's ``--json``, narrowed with ``BENCH_ARGS`` to the flagged
components if you like. Then a row fails only when it is SLOWER in BOTH
runs; one SLOWER in a single run is UNCONFIRMED and passes. A row the first
run flagged that the rerun did not measure is UNCHECKED and fails: absent
is not a pass. See `confirm`.

A renamed family is paired through ``benchmarks/renames.toml`` (#2900),
the table the benchmarks page and the label gate read: every row is keyed
by the ID it has NOW (``c:<component>::<row>``, ``py:<module>::<test>``),
so A's ``c:acq::<row>`` is judged against B's ``c:correlator::<row>``,
suffix for suffix, with nothing declared per run. Within a family a rename
put in force, a row on one side only is UNPAIRED and fails, and two A rows
the table makes one refuse the run.

Examples
--------
    make bench-ab A=origin/main B=HEAD                     # whole suite, K=5
    make bench-ab A=origin/main B=HEAD K=7 \\
        BENCH_ARGS="--c-only async_dsss_pool"              # one C component
    make bench-ab A=v0.78.0 B=HEAD BUILD=native JSON=ab.json
    make bench-ab A=origin/main B=HEAD CONFIRM=ab.json \\
        BENCH_ARGS="--c-only tonemeas"                     # confirm a row
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
from bench_report import (
    canon,
    fastest_cpus,
    machine_not_ready,
    read_renames,
    row_id,
)


def worktree_path(side: str) -> str:
    """A fresh path for one side's worktree, unique to this run.

    A fixed path let two concurrent runs remove each other's checkout
    (#2771). The directory is made and removed again so `git worktree add`
    can create it; the name stays this run's.
    """
    d = tempfile.mkdtemp(prefix=f"doppler-bench-ab-{side}-")
    os.rmdir(d)
    return d


def best_rounds(snaps: list, suite: str | None = None) -> dict:
    """{row: [best round of each pass]} over K snapshots of one suite.

    With ``suite`` (``"c"`` or ``"py"``) a row is named by the page's row
    ID (`bench_report.row_id`), which a moved bench file does not change;
    without it, by its ``fullname``, falling back to ``name``. A pass whose
    row has no positive ``stats.min`` contributes nothing for that row.

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
            if suite and b.get("name"):
                key = row_id("c" if suite == "c" else "python", b)
            elif suite:
                key = f"{suite}:{b.get('fullname')}"
            else:
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


def apply_renames(a: dict, b: dict, renames: dict) -> tuple:
    """Both sides' rows under the IDs they have now; which rows a rename
    in force governs.

    Returns ``(a_now, b_now, governed, problems)``:

    - ``a_now`` and ``b_now`` key every row by `bench_report.canon`, so a
      row A measured under a family the table renamed pairs with B's row
      of the new name;
    - ``governed`` maps each key a rename in force covers -- every renamed
      A row, and every B row in a family some A row was renamed into -- to
      the name A measured it under, or ``None`` for a B row A never had;
    - ``problems`` names two rows of one side that the table makes one.

    >>> a = {"c:corr2d::x": [1.0], "c:fft::y": [1.0]}
    >>> b = {"c:correlator::x": [1.0], "c:fft::y": [1.0]}
    >>> an, bn, gov, bad = apply_renames(a, b, {"c:corr2d": "c:correlator"})
    >>> sorted(an), gov, bad
    (['c:correlator::x', 'c:fft::y'], {'c:correlator::x': 'c:corr2d::x'}, [])
    """
    out, problems, governed, into = [], [], {}, set()
    for side, rows in (("A", a), ("B", b)):
        now: dict = {}
        for k, v in rows.items():
            n = canon(k, renames)
            if n in now:
                problems.append(
                    f"{side} measured both {now[n][0]} and {k}, which "
                    f"benchmarks/renames.toml makes one row, {n}"
                )
                continue
            now[n] = (k, v)
            if side == "A" and n != k:
                governed[n] = k
                into.add(n.partition("::")[0])
        out.append({n: v for n, (_, v) in now.items()})
    for k in out[1]:
        if k.partition("::")[0] in into:
            governed.setdefault(k, None)
    return out[0], out[1], governed, problems


def pair_governed(result: dict, governed: dict) -> dict:
    """Refuse every renamed row that did not pair: absent is not a pass.

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
    ...         "c": {"verdict": "UNCHECKED"},
    ...         "f": {"verdict": "UNCONFIRMED"},
    ...     }
    ... )
    ['y', 'z', 'u', 'c']
    """
    return [r for r, v in result.items() if v["verdict"] in FAILING]


#: The verdicts that fail the run. UNCONFIRMED (SLOWER in one run of two)
#: is not one of them; UNCHECKED (flagged, then never re-measured) is.
FAILING = ("SLOWER", "MISSING", "UNPAIRED", "UNCHECKED")

#: A first run's verdicts that mean "this row has read SLOWER before". A
#: confirm run's own JSON can be the next run's first, and there a row
#: flagged once reads UNCONFIRMED or UNCHECKED, not SLOWER: counting only
#: SLOWER would let a row slow in runs 1 and 3 pass on run 3.
FLAGGED = ("SLOWER", "UNCONFIRMED", "UNCHECKED")


def confirm(result: dict, prior: dict) -> dict:
    """This run's verdicts, held against a first run of the same pair.

    A SLOWER row stands only when the first run (`prior`, its ``rows``)
    also flagged it -- SLOWER, or UNCONFIRMED or UNCHECKED when the first
    run was itself a confirm run (`FLAGGED`); then it carries
    ``confirmed``. SLOWER now but TIE or
    FASTER then is UNCONFIRMED, which passes: a chance crossing of the
    tolerance does not repeat on the same row. A row this run measured that
    the first did not has nothing to confirm it, so it keeps its verdict.

    A row the first run read SLOWER that this run did not measure (a rerun
    narrowed with ``BENCH_ARGS`` to other components) is UNCHECKED, which
    fails. Every other row only the first run measured keeps its first
    verdict, so the table is the whole suite either way.

    >>> first = {
    ...     "noise": {"verdict": "SLOWER"},
    ...     "real": {"verdict": "SLOWER"},
    ...     "skipped": {"verdict": "SLOWER"},
    ...     "kept": {"verdict": "TIE", "ratio": 1.0},
    ... }
    >>> now = {
    ...     "noise": {"verdict": "TIE", "ratio": 1.0},
    ...     "real": {"verdict": "SLOWER", "ratio": 1.1},
    ...     "fresh": {"verdict": "SLOWER", "ratio": 1.1},
    ... }
    >>> r = confirm(now, first)
    >>> {k: v["verdict"] for k, v in sorted(r.items())}
    ... # doctest: +NORMALIZE_WHITESPACE
    {'fresh': 'SLOWER', 'kept': 'TIE', 'noise': 'TIE', 'real': 'SLOWER',
     'skipped': 'UNCHECKED'}
    >>> r["real"]["confirmed"]
    True
    >>> confirm({"x": {"verdict": "SLOWER"}}, {"x": {"verdict": "TIE"}})["x"][
    ...     "verdict"
    ... ]
    'UNCONFIRMED'
    >>> twice = {"x": {"verdict": "UNCHECKED"}}  # flagged by a confirm run
    >>> again = confirm({"x": {"verdict": "SLOWER"}}, twice)
    >>> again["x"]["verdict"]
    'SLOWER'
    """
    out = {}
    for row, then in prior.items():
        if row in result:
            continue
        if then["verdict"] == "SLOWER":
            out[row] = {**then, "verdict": "UNCHECKED"}
        else:
            out[row] = then
    for row, v in result.items():
        then = prior.get(row, {}).get("verdict")
        if v["verdict"] == "SLOWER" and then in FLAGGED:
            out[row] = {**v, "confirmed": True}
        elif v["verdict"] == "SLOWER" and then is not None:
            out[row] = {**v, "verdict": "UNCONFIRMED", "first": then}
        else:
            out[row] = v
    return dict(sorted(out.items()))


def judge(samples: dict, renames: dict) -> tuple:
    """Both suites' verdicts, rows under today's IDs: (result, problems).

    >>> c = {"benchmarks": [{"name": "old::x", "stats": {"min": 1.0}}]}
    >>> d = {"benchmarks": [{"name": "new::x", "stats": {"min": 1.0}}]}
    >>> s = {"A": {"c": [c] * 3, "py": []}, "B": {"c": [d] * 3, "py": []}}
    >>> r, bad = judge(s, {"c:old": "c:new"})
    >>> r["c:new::x"]["verdict"], r["c:new::x"]["alias_from"], bad
    ('TIE', 'c:old::x', [])
    """
    result: dict = {}
    problems: list = []
    for suite in ("c", "py"):
        a = best_rounds(samples["A"][suite], suite)
        b = best_rounds(samples["B"][suite], suite)
        a, b, governed, bad = apply_renames(a, b, renames)
        problems += bad
        result.update(pair_governed(compare(a, b), governed))
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
        # A renamed row names what A measured it as, so the pairing is in
        # the verdict table and not only in the rename line above it.
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
        "--confirm",
        metavar="FIRST_JSON",
        help="a first run's --json of the same A and B: a row then fails "
        "only if it is SLOWER in both runs (#2984)",
    )
    args = ap.parse_args(argv)
    # The table in THIS checkout: a branch that renames a family declares
    # the rename in the same commit.
    renames = read_renames(REPO)
    if args.k < 3:
        # The band is second-best minus best. At K = 2 that is max - min,
        # which one slow pass widens enough to hide a regression (#2771).
        print(
            "bench-ab: K must be at least 3, or one outlier pass sets the band"
        )
        return 2

    commits = {"A": _resolve(args.a), "B": _resolve(args.b)}
    prior = None
    if args.confirm:
        with open(args.confirm) as fh:
            prior = json.load(fh)
        # The rerun confirms THIS pair at THESE flags. A first run of other
        # commits, or another build, says nothing about it and would clear
        # a row it never measured. K may differ: it only sets the band.
        theirs = (prior.get("a"), prior.get("b"), prior.get("build"))
        ours = (commits["A"], commits["B"], args.build)
        if theirs != ours:
            print(
                f"bench-ab: {args.confirm} compared {theirs[0]} with "
                f"{theirs[1]} ({theirs[2]} build), not {ours[0]} with "
                f"{ours[1]} ({ours[2]} build)"
            )
            return 2
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
    finally:
        _remove_worktrees(wts)

    result, problems = judge(samples, renames)
    olds = {
        v["alias_from"].partition("::")[0]
        for v in result.values()
        if v.get("alias_from")
    }
    for old in sorted(olds):
        print(
            f"rename in force (benchmarks/renames.toml): A {old}::<row> "
            f"is B {renames[old]}::<row>"
        )
    if problems:
        for p in problems:
            print(f"bench-ab: {p}")
        return 1
    if not any("ratio" in v for v in result.values()):
        print("bench-ab: no row was measured on both sides")
        return 1
    if prior is not None:
        result = confirm(result, prior["rows"])
        print(f"confirming against {args.confirm}: a row fails only if it")
        print("is SLOWER in both runs, or flagged then and not rerun")
    print(render(result))
    if args.json:
        with open(args.json, "w") as fh:
            json.dump(
                {
                    "a": commits["A"],
                    "b": commits["B"],
                    "k": args.k,
                    "build": args.build,
                    "renames": renames,
                    "rows": result,
                },
                fh,
                indent=1,
            )
            fh.write("\n")
    bad = failures(result)
    counts = {
        v: sum(r["verdict"] == v for r in result.values())
        for v in (
            "FASTER",
            "TIE",
            "UNCONFIRMED",
            "SLOWER",
            "UNCHECKED",
            "MISSING",
            "UNPAIRED",
            "NEW",
        )
    }
    print(", ".join(f"{n} {v.lower()}" for v, n in counts.items() if n))
    if bad:
        print(f"bench-ab: B fails on {len(bad)} row(s): {', '.join(bad)}")
        if prior is None and any(
            result[r]["verdict"] == "SLOWER" for r in bad
        ):
            first = args.json or "first.json"
            kept = "" if args.json else f" (this run kept none: JSON={first})"
            print(
                "bench-ab: one run cannot tell a SLOWER row from noise; "
                f"rerun with CONFIRM={first}{kept}, and only a row SLOWER "
                "in both runs fails"
            )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
