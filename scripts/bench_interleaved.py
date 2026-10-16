#!/usr/bin/env python3
"""Interleaved portable-vs-native benchmark measurement.

`make bench-publish` measures the two builds in separate runs minutes apart, so
the page's *%Δ* column picks up cross-run system drift (a build looking
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
both columns. Run it on a representative machine, from ONE persistent clone
(docs/dev/release.md §2b). It first clears the previous release's outputs
once main has them, and refuses over any main lacks
(`clear_published_copies`).

Usage::

    make bench-preflight                             # before `git checkout`
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
from bench_report import (
    collect_meta,
    fastest_cpus,
    machine_not_ready,
    missing_components,
)
from check_bench_commits import verdict

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


#: What a release's bench run leaves in the clone it ran from: the snapshot
#: (untracked until its PR merges), the page `make bench-docs` renders from
#: it, and the label files `make bench-labels-fix` and a new row's label
#: change for it (tracked, so left modified; legacy-units.toml is deleted
#: by the first release that records every C row's unit). Paths are
#: relative to the root.
BENCH_OUTPUTS = (
    "benchmarks/published",
    "docs/benchmarks.md",
    "benchmarks/labels.toml",
    "benchmarks/.labels-exempt",
    "benchmarks/legacy-units.toml",
)


def _unit(path: str) -> str:
    """The thing a path belongs to, as a report names it: a snapshot's
    version directory, or the file itself.

    >>> _unit("benchmarks/published/v0.71.0/native-c.json")
    'benchmarks/published/v0.71.0/'
    >>> _unit("docs/benchmarks.md")
    'docs/benchmarks.md'
    """
    parts = path.split("/")
    if path.startswith("benchmarks/published/") and len(parts) > 3:
        return "/".join(parts[:3]) + "/"
    return path


def clear_published_copies(base: str = "origin/main", root=None) -> int:
    """Remove the bench outputs ``base`` already holds; refuse any it lacks.

    Each release is measured in a clone, and the run leaves its snapshot
    there untracked, with ``docs/benchmarks.md`` modified. Once the
    snapshot's PR merges, that copy is redundant, and nothing removed it:
    26 per-release clones, about 10 GB, on the bench box by 2026-10-14.
    In ONE persistent clone it is worse than disk, because ``git checkout``
    of the next release commit refuses while the copy is there, identical
    or not.

    So, for every untracked file and every modified tracked file under
    `BENCH_OUTPUTS`:

    - **On** ``base`` **as the same blob**: a stale copy. Each file is
      hashed with its path's filters (``.gitattributes``, ``core.autocrlf``)
      the way ``git status`` compares, so a CRLF-only copy of main's file on
      a Windows checkout is the same blob. The untracked file is removed and
      the tracked one restored (``git checkout -- <file>``), which loses
      nothing, because ``base`` has the content. What was cleared is
      printed.
    - **Absent from** ``base`` **or different**: an unpublished result.
      Nothing is measured over it; the run is refused, with what to do,
      and no file is touched, stale ones included. A file STAGED but not
      committed, and a symlink, are refused the same way: checkout would
      carry the one into the next commit, and the other cannot be hashed
      as a file.

    ``base``'s remote is fetched first, and a failed fetch refuses: a stale
    ``base`` would call a published snapshot unpublished, or the reverse.

    Returns 0 when nothing unpublished remains, else 2.
    """
    root = root or REPO
    remotes = _run(["git", "remote"], root, capture_output=True)
    remote = next(
        (r for r in remotes.stdout.split() if base.startswith(r + "/")), None
    )
    if remote:
        f = _run(
            ["git", "fetch", "--quiet", remote], root, capture_output=True
        )
        if f.returncode != 0:
            print(
                f"bench-preflight: `git fetch {remote}` failed, so whether a "
                f"snapshot is on {base} cannot be known:\n"
                f"{(f.stderr or f.stdout).strip()}"
            )
            return 2
    ok = _run(
        ["git", "rev-parse", "--verify", "--quiet", base + "^{commit}"],
        root,
        capture_output=True,
    )
    if ok.returncode != 0:
        print(f"bench-preflight: {base} does not name a commit here")
        return 2

    def lines(*args):
        out = _run(
            ["git", *args, "--", *BENCH_OUTPUTS], root, capture_output=True
        )
        return [p for p in out.stdout.split("\0") if p]

    untracked = lines("ls-files", "-z", "--others", "--exclude-standard")
    # Working tree against the INDEX, the same pair `git checkout -- f`
    # restores between: what is staged is never what this drops.
    modified = lines("diff", "-z", "--name-only", "--diff-filter=M")
    # Staged and not committed: neither untracked nor different from the
    # index, so neither list above sees it, and `git checkout` carries it
    # into the next commit. It is refused, never cleared.
    staged = lines("diff", "-z", "--cached", "--name-only", "HEAD")
    paths = list(dict.fromkeys(untracked + modified + staged))
    if not paths:
        print(
            f"bench-preflight: no bench outputs here that {base} lacks "
            f"(checked {', '.join(BENCH_OUTPUTS)})"
        )
        return 0

    tree = {}
    for entry in lines("ls-tree", "-r", "-z", base):
        meta, path = entry.split("\t", 1)
        tree[path] = meta.split()[2]
    # Git lists a symlink as a file and hash-object follows it, so a
    # dangling one, or one to a directory, cannot be hashed: refused by name.
    regular = [
        p
        for p in paths
        if p not in staged and not os.path.islink(os.path.join(root, p))
    ]
    hashed = {}
    if regular:
        # WITH the paths' filters, as `git status` compares. Raw bytes
        # (--no-filters) made every CRLF copy of an LF blob "differ" on a
        # Windows checkout, so the gate refused everything there.
        h = _run(
            ["git", "hash-object", "--stdin-paths"],
            root,
            input="\n".join(regular) + "\n",
            capture_output=True,
        )
        if h.returncode != 0:
            print(
                "bench-preflight: cannot read a bench output to compare it "
                f"with {base}, so nothing was touched:\n{h.stderr.strip()}"
            )
            return 2
        hashed = dict(zip(regular, h.stdout.split()))

    units = {}  # unit -> [(path, why-not-stale or None)]
    for path in paths:
        why = None
        if path in staged:
            why = f"{path} is staged and not committed"
        elif path not in hashed:
            why = f"{path} is a symlink"
        elif path not in tree:
            why = f"{path} is not on {base}"
        elif tree[path] != hashed[path]:
            why = f"{path} differs from {base}'s"
        units.setdefault(_unit(path), []).append((path, why))

    refused = {u: [w for _, w in ps if w] for u, ps in units.items()}
    refused = {u: ws for u, ws in refused.items() if ws}
    if refused:  # all or nothing: a refusal touches no file
        print(
            f"bench-preflight: results here that {base} does not clear; "
            "nothing was touched:"
        )
        for unit, whys in refused.items():
            print(f"  {unit}")
            for why in whys:
                print(f"    {why}")
        print(
            "Publish each through its PR, or remove it, then run this again."
        )
        return 2
    for unit, ps in units.items():
        for path, _ in ps:
            if path in modified:
                _run(["git", "checkout", "--", path], root, check=True)
            else:
                os.remove(os.path.join(root, path))
        _prune_empty(os.path.join(root, unit.rstrip("/")), root)
        print(
            f"bench-preflight: cleared {unit} ({len(ps)} file(s) {base} "
            "already has, as git compares it)"
        )
    return 0


def _prune_empty(path: str, root: str) -> None:
    """Remove ``path`` and its parents while empty, stopping at ``root``."""
    while os.path.isdir(path) and path != root and not os.listdir(path):
        os.rmdir(path)
        path = os.path.dirname(path)


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

    A failed run stops the whole measurement, and its stderr is replayed
    first (#2062). That is where a benchmark says why it refused -- a
    jm_bench.h row it could not hold, a direct twin that differs from its
    push -- and `jm bench` fails the run on any binary that exits non-zero
    or writes no JSON (just-makeit#2189). Thrown away, a release run died
    on a bare CalledProcessError naming `make bench` and nothing else.
    """
    r = _run(
        ["make", "bench"],
        wt,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        preexec_fn=(lambda: os.sched_setaffinity(0, cpus)) if cpus else None,
    )
    if r.returncode != 0:
        sys.stderr.write(r.stderr)
        raise SystemExit(
            f"bench-interleaved: `make bench` failed in {wt} "
            f"(exit {r.returncode}); its stderr is above"
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
    ap.add_argument("version", nargs="?")
    ap.add_argument("-k", "--passes", type=int, default=5)
    # The ref bench-commits-check holds a stamp to (BENCH_COMMIT_BASE).
    ap.add_argument("--base", default="origin/main")
    ap.add_argument(
        "--preflight",
        action="store_true",
        help="only clear the bench outputs --base already has (make "
        "bench-preflight, run before checking out the next commit)",
    )
    a = ap.parse_args()
    if a.preflight:
        return clear_published_copies(a.base)
    if not a.version:
        ap.error("a version is required unless --preflight is given")
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

    # The last release's outputs, before this one writes its own beside
    # them: a copy main has goes, an unpublished one stops the run.
    if clear_published_copies(a.base) != 0:
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

    # Every C benchmark the tree has must be in each build's merged set. jm
    # now fails a run whose binary exits non-zero or writes no JSON
    # (just-makeit#2189), and bench_once stops on that; this is the backstop
    # for a set short of the tree any other way, which would otherwise
    # publish short without saying so.
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
    print(_provenance(commit, a.base))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
