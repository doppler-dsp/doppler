#!/usr/bin/env python3
"""Interleaved portable-vs-native benchmark measurement.

`make bench-publish` measures the two builds in separate runs minutes apart, so
the page's *from src* column picks up cross-run system drift (a build looking
slower than itself). This measures both builds **interleaved** and keeps the
per-benchmark best, which cancels that drift:

1. check out the current commit into two throwaway git worktrees and build each
   — one **portable** (the wheel baseline), one **native**
   (``-DDOPPLER_NATIVE=ON``). Worktrees give each build its own ``.so`` and C
   bench binaries, so they can't collide;
2. run the full suite alternately, portable / native / portable / …, K times
   (order flipped each round so neither build is systematically favoured);
3. per benchmark, keep the run with the **lowest mean** (the interference-free
   sample) — its ``MSa_s`` is already consistent with that mean;
4. stamp each merged snapshot with full reproducibility metadata (when,
   commit, compiler + flags, CPU state, lib versions — see
   ``bench_report.collect_meta``) and write it to
   ``benchmarks/published/v<version>/``.

This replaces the two manual `bench-publish` passes — one command publishes
both columns. Run it on a representative machine.

Usage::

    make bench-interleaved VERSION=0.10.1            # K=5 passes
    python scripts/bench_interleaved.py 0.10.1 -k 7
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench_report import (
    collect_meta,
    fastest_cpus,
    machine_not_ready,
    missing_components,
)

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PUBLISHED = os.path.join(REPO, "benchmarks", "published")
#: The tree's C benchmarks, the set a published C snapshot must cover.
BENCH_SRC = os.path.join(REPO, "native", "benchmarks")
BUILD_ARGS = {"portable": [], "native": ["-DDOPPLER_NATIVE=ON"]}
FLAG_RE = re.compile(
    r"-O\S+|-march=\S+|-mtune=\S+|-mprefer-vector-width=\S+|-ffast-math|-funsafe-math\S*"
)


def _run(cmd, cwd, **kw):
    return subprocess.run(cmd, cwd=cwd, text=True, **kw)


def bench_args_set(environ=None) -> str:
    """The BENCH_ARGS a release measurement would inherit, or "" if none.

    Every inner ``make bench`` this script runs reads MAKEFLAGS, and
    ``make bench-interleaved VERSION=X BENCH_ARGS="--c-only conv"`` puts
    the variable there -- so the release would be measured over a filtered
    set and published as if it were the whole suite (#1975's review). An
    exported BENCH_ARGS reaches it through the environment the same way.
    An empty value is harmless and reads as unset.

    >>> bench_args_set({"MAKEFLAGS": " -- VERSION=1 BENCH_ARGS=--c-only"})
    '--c-only'
    >>> bench_args_set({"MAKEFLAGS": " -- VERSION=1 BENCH_ARGS="})
    ''
    >>> bench_args_set({"BENCH_ARGS": "rs"})
    'rs'
    >>> bench_args_set({})
    ''
    """
    env = os.environ if environ is None else environ
    if env.get("BENCH_ARGS", "").strip():
        return env["BENCH_ARGS"].strip()
    m = re.search(r"(?:^|\s)BENCH_ARGS=(\S*)", env.get("MAKEFLAGS", ""))
    return m.group(1) if m else ""


def _remove_worktrees(wts):
    for wt in wts.values():
        _run(
            ["git", "worktree", "remove", "--force", wt],
            REPO,
            capture_output=True,
        )


def _build_info(worktree):
    """(compiler, flags) from the worktree's compile_commands.json."""
    db = os.path.join(worktree, "build", "compile_commands.json")
    if not os.path.exists(db):
        return "", ""
    with open(db) as fh:
        entries = json.load(fh)
    core = next(
        (
            e
            for e in entries
            if e["file"].endswith("_core.c") and "vendor" not in e["file"]
        ),
        entries[0] if entries else None,
    )
    cmd = core.get("command") or " ".join(core.get("arguments", []))
    cc = cmd.split()[0]
    v = _run([cc, "--version"], REPO, capture_output=True)
    compiler = v.stdout.splitlines()[0].strip() if v.returncode == 0 else cc
    return compiler, " ".join(dict.fromkeys(FLAG_RE.findall(cmd)))


def setup_worktree(build):
    wt = f"/tmp/doppler-bench-{build}"
    _run(
        ["git", "worktree", "remove", "--force", wt], REPO, capture_output=True
    )
    _run(
        ["git", "worktree", "add", "-f", "--detach", wt, "HEAD"],
        REPO,
        check=True,
        capture_output=True,
    )
    print(f"[{build}] building in {wt} ...", flush=True)
    _run(
        ["make", "pyext", "CMAKE_ARGS=" + " ".join(BUILD_ARGS[build])],
        wt,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return wt


def bench_once(wt, cpus=None):
    """Run the suite once in the worktree; return (python_json, c_json).

    `cpus`, when given, is the affinity the MEASUREMENT runs under -- the
    fastest core class (`bench_report.fastest_cpus`). Only this call is
    pinned: the builds in `setup_worktree` keep every core, because a build is
    not being measured. The affinity is set in the child before exec, so
    `make`, pytest and every benchmark binary below it inherit it.
    """
    _run(
        ["make", "bench"],
        wt,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        preexec_fn=(lambda: os.sched_setaffinity(0, cpus)) if cpus else None,
    )
    hist = os.path.join(wt, "benchmarks", "history")
    pj = max(
        (f for f in glob.glob(hist + "/*.json") if not f.endswith("-c.json")),
        key=os.path.getmtime,
    )
    tag = os.path.basename(pj)[: -len(".json")]
    return json.load(open(pj)), json.load(open(f"{hist}/{tag}-c.json"))


def _merge_best(snaps):
    """Per benchmark across the K snapshots, keep the entry with the lowest
    stats.mean (its MSa_s is already from that mean). Returns a snapshot."""
    best = {}
    for s in snaps:
        for b in s.get("benchmarks", []):
            key = b.get("fullname") or b["name"]
            m = b.get("stats", {}).get("mean")
            if not isinstance(m, (int, float)) or m <= 0:
                continue
            if key not in best or m < best[key]["stats"]["mean"]:
                best[key] = b
    merged = dict(snaps[-1])  # machine_info, datetime, etc. from last pass
    merged["benchmarks"] = list(best.values())
    return merged


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("version")
    ap.add_argument("-k", "--passes", type=int, default=5)
    a = ap.parse_args()
    ver = "v" + a.version.lstrip("v")

    # A release is measured over the WHOLE suite: refuse a filter before any
    # building, rather than publish a partial set as if it were complete.
    leaked = bench_args_set()
    if leaked:
        print(
            f"bench-interleaved: BENCH_ARGS={leaked} is set and every inner "
            "`make bench` would inherit it; a release is measured over the "
            "whole suite. Run it without BENCH_ARGS."
        )
        return 2

    # Refuse BEFORE the hour of building and measuring, not after: a snapshot
    # taken in the wrong state publishes numbers no later release can be
    # compared against, and the page would not say so.
    problems = machine_not_ready()
    if problems:
        print("bench-interleaved: this machine is not ready to measure:")
        for p in problems:
            print(f"  - {p}")
        return 2

    wts = {b: setup_worktree(b) for b in BUILD_ARGS}
    info = {b: _build_info(wts[b]) for b in BUILD_ARGS}
    samples = {b: {"py": [], "c": []} for b in BUILD_ARGS}
    cpus = fastest_cpus()
    print(
        f"measuring on cpus {cpus} (the fastest core class)"
        if cpus
        else "measuring unpinned (one core class, or no cpufreq to read)",
        flush=True,
    )
    order = list(BUILD_ARGS)
    for i in range(a.passes):
        for b in order if i % 2 == 0 else order[::-1]:
            print(f"pass {i + 1}/{a.passes} [{b}] ...", flush=True)
            py, c = bench_once(wts[b], cpus)
            samples[b]["py"].append(py)
            samples[b]["c"].append(c)

    # Every C benchmark the tree has must be in each build's merged set: a
    # bench that crashed in every pass wrote no JSON, and jm skips a binary
    # with none, so the snapshot would be short without saying so.
    short = {
        b: missing_components(_merge_best(samples[b]["c"]), BENCH_SRC)
        for b in BUILD_ARGS
    }
    short = {b: m for b, m in short.items() if m}
    if short:
        for b, m in short.items():
            print(
                f"bench-interleaved: refusing to publish -- the {b} passes "
                f"never recorded {len(m)} of the tree's C benchmarks: "
                f"{', '.join(m)}"
            )
        _remove_worktrees(wts)
        return 1

    commit = _run(
        ["git", "rev-parse", "--short", "HEAD"], REPO, capture_output=True
    ).stdout.strip()
    dst = os.path.join(PUBLISHED, ver)
    os.makedirs(dst, exist_ok=True)
    for b in BUILD_ARGS:
        compiler, flags = info[b]
        for suite, name in (("py", f"{b}.json"), ("c", f"{b}-c.json")):
            merged = _merge_best(samples[b][suite])
            merged["doppler_meta"] = collect_meta(
                merged.get("machine_info", {}),
                compiler,
                flags,
                commit,
                merged.get("datetime", ""),
                pinned_cpus=cpus,
            )
            with open(os.path.join(dst, name), "w") as fh:
                json.dump(merged, fh, indent=1)
                fh.write("\n")
        print(f"published {ver}/{b}  [{compiler}; {flags}]")

    _remove_worktrees(wts)
    print(f"done — {a.passes} interleaved passes per build")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
