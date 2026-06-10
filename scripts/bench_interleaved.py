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
4. stamp each merged snapshot with the build's compiler + flags and write it to
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

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PUBLISHED = os.path.join(REPO, "benchmarks", "published")
BUILD_ARGS = {"portable": [], "native": ["-DDOPPLER_NATIVE=ON"]}
FLAG_RE = re.compile(
    r"-O\S+|-march=\S+|-mtune=\S+|-ffast-math|-funsafe-math\S*"
)


def _run(cmd, cwd, **kw):
    return subprocess.run(cmd, cwd=cwd, text=True, **kw)


def _build_info(worktree):
    """(compiler, flags) from the worktree's compile_commands.json."""
    db = os.path.join(worktree, "build", "compile_commands.json")
    if not os.path.exists(db):
        return "", ""
    entries = json.load(open(db))
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


def bench_once(wt):
    """Run the suite once in the worktree; return (python_json, c_json)."""
    _run(
        ["make", "bench"],
        wt,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
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

    wts = {b: setup_worktree(b) for b in BUILD_ARGS}
    info = {b: _build_info(wts[b]) for b in BUILD_ARGS}
    samples = {b: {"py": [], "c": []} for b in BUILD_ARGS}
    order = list(BUILD_ARGS)
    for i in range(a.passes):
        for b in order if i % 2 == 0 else order[::-1]:
            print(f"pass {i + 1}/{a.passes} [{b}] ...", flush=True)
            py, c = bench_once(wts[b])
            samples[b]["py"].append(py)
            samples[b]["c"].append(c)

    dst = os.path.join(PUBLISHED, ver)
    os.makedirs(dst, exist_ok=True)
    for b in BUILD_ARGS:
        compiler, flags = info[b]
        for suite, name in (("py", f"{b}.json"), ("c", f"{b}-c.json")):
            merged = _merge_best(samples[b][suite])
            merged["doppler_build"] = {"compiler": compiler, "flags": flags}
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
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
