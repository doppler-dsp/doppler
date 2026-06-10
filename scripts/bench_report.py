#!/usr/bin/env python3
"""Read doppler's benchmark snapshots into a table.

Two sources, two purposes:

* **`--static`** (``make bench-table``) — a representative absolute-numbers
  table from THIS machine's latest ``make bench`` run
  (``benchmarks/history/``), stamped with the CPU it ran on. These are the
  numbers you quote in the docs/README: a dedicated box, not a shared CI
  runner.
* **default** (``make bench-report``) — a newest-vs-previous **comparison**
  table read from the release snapshots on the **`benchmarks` branch**
  (committed by ``benchmark.yml`` per tag). Useful for spotting movement, but
  GitHub runners are shared/non-deterministic, so treat these as indicative
  only — never publish them.

Metric. Throughput benchmarks record samples/s in
``extra_info["MSa_s"]`` — shown as **MSa/s, higher is better**. Benchmarks
with no sample count (scalar ops; the whole C suite today, whose harness
records no size) fall back to **mean time per call, lower is better**.

Examples
--------
::

    python scripts/bench_report.py --static        # representative table
    python scripts/bench_report.py                 # CI-branch comparison
    python scripts/bench_report.py --top 40
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from collections import defaultdict

BRANCH = "origin/benchmarks"
VER_RE = re.compile(r"^v(\d+)\.(\d+)\.(\d+)$")
SUITES = ("python", "c")
SUITE_LABEL = {"c": "C (jm_bench)", "python": "Python (pytest-benchmark)"}


def _git(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["git", *args], capture_output=True, text=True)


def _version_key(tag: str):
    """(major, minor, patch) for a ``vX.Y.Z`` tag, else None (skips baselines)."""
    m = VER_RE.match(tag)
    return tuple(int(x) for x in m.groups()) if m else None


def _display_name(suite: str, bench: dict) -> str:
    """Short, unique label. Python `name`s collide across modules (the same
    ``test_bench_steps[1024]`` lives in several files), so derive a
    ``module::case`` label from ``fullname`` for Python; C names are already
    unique and clean (``RateConverter::HB(0.5)``)."""
    name = bench.get("name", "?")
    full = bench.get("fullname") or name
    if suite == "python" and "::" in full:
        mod = full.rsplit("::", 1)[0].rsplit("/", 1)[-1]
        mod = mod.removesuffix(".py").removeprefix("bench_")
        case = name.removeprefix("test_bench_").removeprefix("test_")
        return f"{mod}::{case}" if mod else case
    return name


def load_history():
    """Load every release snapshot from the benchmarks branch.

    Returns ``{suite: {tag: {fullname: {"disp", "mean", "msas"}}}}`` with tags
    sorted ascending by version. ``msas`` is None when the benchmark records no
    throughput. Non-version snapshots (e.g. ``baseline-*``) are skipped.
    """
    _git("fetch", "-q", "origin", "benchmarks")
    listing = _git("ls-tree", "-r", "--name-only", BRANCH)
    series = {s: defaultdict(dict) for s in SUITES}
    if listing.returncode != 0:
        return {s: {} for s in SUITES}  # branch unavailable (shallow clone)

    for path in listing.stdout.splitlines():
        if not (path.startswith("history/") and path.endswith(".json")):
            continue
        fn = path[len("history/") :]
        if fn.endswith("-c.json"):
            tag, suite = fn[: -len("-c.json")], "c"
        else:
            tag, suite = fn[: -len(".json")], "python"
        if _version_key(tag) is None:
            continue
        blob = _git("show", f"{BRANCH}:{path}")
        if blob.returncode != 0:
            continue
        for b in json.loads(blob.stdout).get("benchmarks", []):
            mean = b.get("stats", {}).get("mean")
            if not (isinstance(mean, (int, float)) and mean > 0):
                continue
            msas = b.get("extra_info", {}).get("MSa_s")
            series[suite][tag][b.get("fullname") or b["name"]] = {
                "disp": _display_name(suite, b),
                "mean": float(mean),
                "msas": float(msas)
                if isinstance(msas, (int, float))
                else None,
            }

    return {
        s: dict(sorted(d.items(), key=lambda kv: _version_key(kv[0])))
        for s, d in series.items()
    }


def load_local(history_dir="benchmarks/history"):
    """Read the newest LOCAL snapshot pair (this machine's ``make bench``
    output) from ``benchmarks/history/``.

    This is the *representative* path: the numbers come from whatever box ran
    ``make bench``, not the shared, non-deterministic CI runner the
    ``benchmarks`` branch holds. Returns ``{tag, machine, datetime, suites:
    {python, c}}`` or None when there is no local run yet.
    """
    import glob
    import os

    py = sorted(
        (
            f
            for f in glob.glob(os.path.join(history_dir, "*.json"))
            if not f.endswith("-c.json")
        ),
        key=os.path.getmtime,
    )
    if not py:
        return None
    latest = py[-1]
    tag = os.path.basename(latest)[: -len(".json")]
    suites = {"python": {}, "c": {}}

    def _parse(data, suite):
        for b in data.get("benchmarks", []):
            mean = b.get("stats", {}).get("mean")
            if not (isinstance(mean, (int, float)) and mean > 0):
                continue
            msas = b.get("extra_info", {}).get("MSa_s")
            suites[suite][b.get("fullname") or b["name"]] = {
                "disp": _display_name(suite, b),
                "mean": float(mean),
                "msas": float(msas)
                if isinstance(msas, (int, float))
                else None,
            }

    with open(latest) as fh:
        pdata = json.load(fh)
    _parse(pdata, "python")
    cfile = os.path.join(history_dir, f"{tag}-c.json")
    if os.path.exists(cfile):
        with open(cfile) as fh:
            _parse(json.load(fh), "c")
    return {
        "tag": tag,
        "machine": pdata.get("machine_info", {}),
        "datetime": pdata.get("datetime", ""),
        "suites": suites,
    }


def cmd_static(local, out_path=None) -> int:
    """Render a representative absolute-numbers benchmark page for one local run.

    Single-machine snapshot (no cross-release deltas, no CI): doppler's quoted
    performance, with the CPU it was measured on stamped in the header so it is
    never mistaken for the shared CI runner. Written to ``out_path`` (the
    committed docs page) when given, else printed.
    """
    if not local:
        msg = (
            "No local snapshot under benchmarks/history/ — run `make bench` "
            "on a representative machine first, then `make bench-docs`."
        )
        print(msg)
        return 1
    cpu = local["machine"].get("cpu")
    cpu = cpu.get("brand_raw") if isinstance(cpu, dict) else cpu
    pyv = local["machine"].get("python_version", "?")
    when = (local["datetime"] or "")[:10]
    out = [
        "<!-- generated by `make bench-docs` (scripts/bench_report.py "
        "--static); regenerate on a representative machine, don't hand-edit -->",
        "# Benchmarks",
        "",
        f"Measured on **{cpu or 'this machine'}**, Python {pyv}"
        + (f", {when}" if when else "")
        + ". Throughput is **MSa/s** (higher is better); other ops are mean "
        "**time per call** (lower is better). These are representative "
        "single-machine numbers — *not* the shared CI runner (whose absolute "
        "values aren't hardware-representative). Regenerate with "
        "`make bench && make bench-docs`.",
        "",
    ]
    for suite in SUITES:
        benches = local["suites"].get(suite, {})
        if not benches:
            continue
        out += [f"## {SUITE_LABEL[suite]}", ""]
        tput = sorted(
            (e for e in benches.values() if e["msas"] is not None),
            key=lambda e: -e["msas"],
        )
        timed = sorted(
            (e for e in benches.values() if e["msas"] is None),
            key=lambda e: e["mean"],
        )
        if tput:
            out += ["| benchmark | throughput |", "| --- | ---: |"]
            out += [f"| `{e['disp']}` | {fmt_tput(e['msas'])} |" for e in tput]
            out.append("")
        if timed:
            out += ["| benchmark | time/call |", "| --- | ---: |"]
            out += [
                f"| `{e['disp']}` | {fmt_time(e['mean'])} |" for e in timed
            ]
            out.append("")
    text = "\n".join(out).rstrip("\n")
    if out_path:
        with open(out_path, "w") as fh:
            fh.write(text + "\n")  # exactly one trailing newline (idempotent)
        print(f"wrote {out_path} ({cpu})")
    else:
        print(text)
    return 0


def fmt_time(seconds: float) -> str:
    for scale, unit in ((1e-9, "ns"), (1e-6, "µs"), (1e-3, "ms"), (1.0, "s")):
        if seconds < scale * 1000:
            return f"{seconds / scale:.2f} {unit}"
    return f"{seconds:.2f} s"


def fmt_tput(msas: float) -> str:
    """MSa/s, or GSa/s once it crosses 1000."""
    return f"{msas / 1000:.2f} GSa/s" if msas >= 1000 else f"{msas:.1f} MSa/s"


def _groups(snaps: dict):
    """Split benchmark keys into (throughput, timed) by whether any snapshot
    recorded MSa/s for them."""
    tput, timed = set(), set()
    for tag in snaps:
        for key, e in snaps[tag].items():
            (tput if e["msas"] is not None else timed).add(key)
    timed -= tput  # a key with MSa/s in any snapshot counts as throughput
    return tput, timed


def _value(entry: dict, throughput: bool):
    return entry["msas"] if throughput else entry["mean"]


def _rows(tags, snaps, keys, throughput):
    """(disp, prev, cur, pct) for keys present (with a value) in both newest
    tags, sorted by |pct| desc. pct is the signed change in the metric."""
    prev_t, cur_t = tags[-2], tags[-1]
    rows = []
    for k in keys:
        pe, ce = snaps[prev_t].get(k), snaps[cur_t].get(k)
        if not pe or not ce:
            continue
        a, b = _value(pe, throughput), _value(ce, throughput)
        if a and b:
            rows.append((ce["disp"], a, b, (b - a) / a * 100.0))
    rows.sort(key=lambda r: -abs(r[3]))
    return prev_t, cur_t, rows


def _improved(pct: float, throughput: bool, thr: float) -> int:
    """+1 better, -1 worse, 0 within threshold (direction depends on metric)."""
    good = pct > thr if throughput else pct < -thr
    bad = pct < -thr if throughput else pct > thr
    return 1 if good else (-1 if bad else 0)


def _section_iter(series):
    """Yield (suite, label, throughput, keys, snaps) for each non-empty group."""
    for suite in SUITES:
        snaps = series[suite]
        if len(snaps) < 2:
            continue
        tput, timed = _groups(snaps)
        for throughput, keys in ((True, tput), (False, timed)):
            if keys:
                metric = "MSa/s ↑" if throughput else "time ↓"
                yield (
                    suite,
                    f"{SUITE_LABEL[suite]} — {metric}",
                    throughput,
                    keys,
                    snaps,
                )


def cmd_table(series, top: int, thr: float) -> int:
    any_data = False
    for _suite, label, tput, keys, snaps in _section_iter(series):
        any_data = True
        prev, cur, rows = _rows(list(snaps), snaps, keys, tput)
        if not rows:
            continue
        fmt = fmt_tput if tput else fmt_time
        better = sum(1 for *_, p in rows if _improved(p, tput, thr) > 0)
        worse = sum(1 for *_, p in rows if _improved(p, tput, thr) < 0)
        print(
            f"\n=== {label}: {cur} vs {prev} "
            f"({len(rows)} benchmarks, ±{thr:.0f}%) ==="
        )
        print(f"{'benchmark':46}  {prev:>11}  {cur:>11}  {'Δ':>8}")
        print("-" * 82)
        for disp, a, b, pct in rows[:top]:
            mark = _improved(pct, tput, thr)
            flag = " ✓" if mark > 0 else (" ⚠" if mark < 0 else "")
            d = disp if len(disp) <= 46 else disp[:43] + "..."
            print(f"{d:46}  {fmt(a):>11}  {fmt(b):>11}  {pct:>+7.1f}%{flag}")
        if len(rows) > top:
            print(f"... {len(rows) - top} more (--top {len(rows)} for all)")
        print(
            f"summary: {better} better, {worse} worse "
            f"(>{thr:.0f}%); ✓ = faster"
        )
    if not any_data:
        print(
            "No release snapshots on the `benchmarks` branch (need ≥2 "
            "releases). Is origin/benchmarks fetched?"
        )
        return 1
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument(
        "--static",
        action="store_true",
        help="representative absolute-numbers table from THIS machine's "
        "latest `make bench` run (benchmarks/history/), not the CI branch",
    )
    p.add_argument(
        "--out",
        default=None,
        help="write the --static page here (e.g. docs/benchmarks.md) instead "
        "of stdout",
    )
    p.add_argument(
        "--top",
        type=int,
        default=20,
        help="rows/series per group, comparison mode (default 20)",
    )
    p.add_argument(
        "--threshold",
        type=float,
        default=10.0,
        help="±%% to flag better/worse (comparison mode)",
    )
    a = p.parse_args()
    if a.static:
        return cmd_static(load_local(), a.out)
    return cmd_table(load_history(), a.top, a.threshold)


if __name__ == "__main__":
    raise SystemExit(main())
