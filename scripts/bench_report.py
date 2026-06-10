#!/usr/bin/env python3
"""Render doppler's benchmark history into a comparison table and/or a docs page.

The release pipeline (`.github/workflows/benchmark.yml`) commits one Python
(`<tag>.json`, pytest-benchmark) and one C (`<tag>-c.json`, jm_bench) snapshot
per release tag to the **`benchmarks` branch** under `history/`. Those snapshots
were write-only — nothing read them back. This is that reader: it turns the
release time-series into a table you can scan and a docs page you can look at.

Metric. Throughput benchmarks record samples-per-second in
``stats``-adjacent ``extra_info["MSa_s"]`` (mega-samples/s) — for those, the
headline number is **MSa/s, higher is better**. Benchmarks without a sample
count (scalar ops; the whole C suite today, whose harness records no size) fall
back to **mean time per call, lower is better**. Each report labels which.

Data source is the git branch itself (read via ``git show``), so it needs no
working-tree checkout of the snapshots — just ``origin/benchmarks`` fetched.

Examples
--------
Newest-release-vs-previous comparison (what ``make bench-report`` runs)::

    python scripts/bench_report.py
    python scripts/bench_report.py --top 40

Docs page + trend plots (run by ``make docs-build``)::

    python scripts/bench_report.py --page \
        --out docs/benchmarks.md --assets docs/assets/bench
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


def cmd_page(series, out: str, assets: str, top: int) -> int:
    import os

    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    os.makedirs(assets, exist_ok=True)
    rel = os.path.relpath(assets, os.path.dirname(out) or ".")
    lines = [
        "# Benchmarks",
        "",
        "Per-release benchmark history, captured automatically on every "
        "release tag by [`benchmark.yml`](https://github.com/doppler-dsp/"
        "doppler/blob/main/.github/workflows/benchmark.yml) on a pinned runner "
        "(ubuntu-24.04, Python 3.12) and stored on the `benchmarks` branch.",
        "",
        "Throughput benchmarks are shown in **MSa/s (higher is better)**; "
        "others in **mean time per call (lower is better)**. Single-release "
        "deltas carry cross-runner CI noise — read the *trend*, not one step.",
        "",
    ]

    produced = False
    seen_suite = set()
    for suite, label, tput, keys, snaps in _section_iter(series):
        produced = True
        if suite not in seen_suite:
            lines += [f"## {SUITE_LABEL[suite]}", ""]
            seen_suite.add(suite)
        tags = list(snaps)
        # The N "biggest" benchmarks in the newest release: fastest by
        # throughput, or slowest by time — the headline numbers worth watching.
        newest = {
            k: _value(snaps[tags[-1]][k], tput)
            for k in keys
            if k in snaps[tags[-1]]
        }
        watch = sorted(newest, key=lambda k: -newest[k])[:top]

        fig, ax = plt.subplots(figsize=(10, 5.5))
        xs = range(len(tags))
        for k in watch:
            ys = [
                _value(snaps[t][k], tput) if k in snaps[t] else None
                for t in tags
            ]
            ax.plot(
                xs,
                [y if y else float("nan") for y in ys],
                marker="o",
                ms=3,
                lw=1.2,
                label=snaps[tags[-1]][k]["disp"],
            )
        ax.set_xticks(list(xs))
        ax.set_xticklabels(tags, rotation=45, ha="right", fontsize=7)
        if tput:
            ax.set_ylabel("throughput (MSa/s) — higher is better")
        else:
            ax.set_ylabel("mean time per call (s) — lower is better")
            ax.set_yscale("log")
        ax.grid(True, which="both", alpha=0.25)
        ax.legend(
            fontsize=6, ncol=1, loc="upper left", bbox_to_anchor=(1.01, 1.0)
        )
        fig.tight_layout()
        slug = f"{suite}-{'tput' if tput else 'time'}"
        fig.savefig(os.path.join(assets, f"trend-{slug}.png"), dpi=110)
        plt.close(fig)

        prev, cur, rows = _rows(tags, snaps, keys, tput)
        fmt = fmt_tput if tput else fmt_time
        unit = "MSa/s, ↑ better" if tput else "time, ↓ better"
        lines += [
            f"### {'Throughput' if tput else 'Timing'} ({unit})",
            "",
            f"![{slug} trend]({rel}/trend-{slug}.png)",
            "",
            f"| benchmark | {prev} | {cur} | Δ |",
            "| --- | ---: | ---: | ---: |",
        ]
        for disp, a, b, pct in rows[:top]:
            lines.append(f"| `{disp}` | {fmt(a)} | {fmt(b)} | {pct:+.1f}% |")
        lines.append("")

    if not produced:
        lines += ["_No release snapshots available at build time._", ""]
    with open(out, "w") as fh:
        fh.write("\n".join(lines))
    print(f"wrote {out}" + ("" if produced else " (no data)"))
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument(
        "--page",
        action="store_true",
        help="generate the docs page + plots instead of a table",
    )
    p.add_argument("--out", default="docs/benchmarks.md")
    p.add_argument("--assets", default="docs/assets/bench")
    p.add_argument(
        "--top",
        type=int,
        default=20,
        help="rows/series per group (default 20)",
    )
    p.add_argument(
        "--threshold",
        type=float,
        default=10.0,
        help="±%% to flag better/worse (table mode)",
    )
    a = p.parse_args()
    series = load_history()
    if a.page:
        return cmd_page(series, a.out, a.assets, a.top)
    return cmd_table(series, a.top, a.threshold)


if __name__ == "__main__":
    raise SystemExit(main())
