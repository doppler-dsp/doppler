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
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench_report import collect_meta, fastest_cpus, machine_not_ready
from check_bench_commits import verdict

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PUBLISHED = os.path.join(REPO, "benchmarks", "published")
BUILD_ARGS = {"portable": [], "native": ["-DDOPPLER_NATIVE=ON"]}
FLAG_RE = re.compile(
    r"-O\S+|-march=\S+|-mtune=\S+|-mprefer-vector-width=\S+|-ffast-math|-funsafe-math\S*"
)


def _run(cmd, cwd, **kw):
    return subprocess.run(cmd, cwd=cwd, text=True, **kw)


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


def setup_worktree(build, commit):
    wt = f"/tmp/doppler-bench-{build}"
    _run(
        ["git", "worktree", "remove", "--force", wt], REPO, capture_output=True
    )
    _run(
        ["git", "worktree", "add", "-f", "--detach", wt, commit],
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


def _provenance(commit: str, base: str) -> str:
    """Which commit is measured, and whether ``base`` has it (#1322).

    `doppler_meta.commit` is the provenance a reader checks out, and
    `make bench-commits-check` refuses one main cannot reach. Said before
    the hour of measuring and again after it, because the usual way to
    miss it is to measure a local commit (section 2's gallery plots,
    committed but not landed) and find out at the publishing PR.
    """
    why = verdict(Path(REPO), commit, base)
    if why is None:
        return f"measuring doppler {commit}, which {base} has"
    return (
        f"!! measuring doppler {commit}, which is {why}.\n"
        "!! bench-commits-check will refuse this set. bench-restamp can\n"
        "!! fix it after the merge ONLY if this exact tree lands on main\n"
        "!! unchanged (a rebase-merge of it, never a squash). To measure\n"
        "!! what main has instead:\n"
        f"!!   git fetch origin && git checkout --detach {base}"
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("version")
    ap.add_argument("-k", "--passes", type=int, default=5)
    # The ref bench-commits-check holds a stamp to (BENCH_COMMIT_BASE).
    ap.add_argument("--base", default="origin/main")
    a = ap.parse_args()
    ver = "v" + a.version.lstrip("v")

    # Refuse BEFORE the hour of building and measuring, not after: a snapshot
    # taken in the wrong state publishes numbers no later release can be
    # compared against, and the page would not say so.
    problems = machine_not_ready()
    if problems:
        print("bench-interleaved: this machine is not ready to measure:")
        for p in problems:
            print(f"  - {p}")
        return 2

    # Read once, before building: the worktrees are made from this HEAD,
    # so the stamp must name it even if HEAD moves during the run.
    commit = _run(
        ["git", "rev-parse", "--short", "HEAD"], REPO, capture_output=True
    ).stdout.strip()
    print(_provenance(commit, a.base), flush=True)

    wts = {b: setup_worktree(b, commit) for b in BUILD_ARGS}
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

    for wt in wts.values():
        _run(
            ["git", "worktree", "remove", "--force", wt],
            REPO,
            capture_output=True,
        )
    print(f"done — {a.passes} interleaved passes per build")
    print(_provenance(commit, a.base))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
