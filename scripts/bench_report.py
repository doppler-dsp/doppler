#!/usr/bin/env python3
"""Render doppler's benchmark history into a comparison table and/or a docs page.

The release pipeline (`.github/workflows/benchmark.yml`) commits one Python
(`<tag>.json`, pytest-benchmark) and one C (`<tag>-c.json`, jm_bench) snapshot
per release tag to the **`benchmarks` branch** under `history/`. Both share the
pytest-benchmark ``stats`` shape (mean/min/stddev in seconds). Those snapshots
were write-only — nothing ever read them back. This is that reader: it turns the
release time-series into a table you can scan and a docs page you can look at.

Data source is the git branch itself (read via ``git show``), so it needs no
working-tree checkout of the snapshots — just ``origin/benchmarks`` fetched.

Examples
--------
Print the newest-release-vs-previous comparison (what ``make bench-report`` runs)::

    python scripts/bench_report.py
    python scripts/bench_report.py --top 40

Generate the docs page + trend plots (run by ``make docs-build``)::

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
SUITES = ("c", "python")
SUITE_LABEL = {"c": "C (jm_bench)", "python": "Python (pytest-benchmark)"}


def _git(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["git", *args], capture_output=True, text=True)


def _version_key(tag: str):
    """(major, minor, patch) for a ``vX.Y.Z`` tag, else None (skips baselines)."""
    m = VER_RE.match(tag)
    return tuple(int(x) for x in m.groups()) if m else None


def load_history() -> dict[str, dict[str, dict[str, float]]]:
    """Load every release snapshot from the benchmarks branch.

    Returns ``{suite: {tag: {benchmark_name: mean_seconds}}}`` with tags sorted
    ascending by semantic version. Non-version snapshots (e.g. ``baseline-*``)
    are skipped so the series is a clean release timeline.
    """
    _git("fetch", "-q", "origin", "benchmarks")
    listing = _git("ls-tree", "-r", "--name-only", BRANCH)
    series: dict[str, dict[str, dict[str, float]]] = {
        s: defaultdict(dict) for s in SUITES
    }
    if listing.returncode != 0:
        return {
            s: {} for s in SUITES
        }  # branch unavailable (e.g. shallow clone)

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
            if isinstance(mean, (int, float)) and mean > 0:
                series[suite][tag][b["name"]] = float(mean)

    return {
        s: dict(sorted(d.items(), key=lambda kv: _version_key(kv[0])))
        for s, d in series.items()
    }


def fmt_time(seconds: float) -> str:
    """Human-readable duration: ns / µs / ms / s."""
    for scale, unit in ((1e-9, "ns"), (1e-6, "µs"), (1e-3, "ms"), (1.0, "s")):
        if seconds < scale * 1000:
            return f"{seconds / scale:.2f} {unit}"
    return f"{seconds:.2f} s"


def _delta_rows(tags: list[str], snaps: dict[str, dict[str, float]]):
    """(name, prev_mean, cur_mean, pct) for benchmarks present in both newest tags."""
    prev, cur = tags[-2], tags[-1]
    rows = []
    for name, cur_mean in snaps[cur].items():
        prev_mean = snaps[prev].get(name)
        if prev_mean:
            pct = (cur_mean - prev_mean) / prev_mean * 100.0
            rows.append((name, prev_mean, cur_mean, pct))
    rows.sort(key=lambda r: -abs(r[3]))
    return prev, cur, rows


def cmd_table(series, top: int, regress_pct: float) -> int:
    """Print a newest-vs-previous comparison table per suite to stdout."""
    any_data = False
    for suite in SUITES:
        snaps = series[suite]
        tags = list(snaps)
        if len(tags) < 2:
            continue
        any_data = True
        prev, cur, rows = _delta_rows(tags, snaps)
        regressions = sum(1 for *_, p in rows if p > regress_pct)
        improvements = sum(1 for *_, p in rows if p < -regress_pct)
        print(
            f"\n=== {SUITE_LABEL[suite]}: {cur} vs {prev} "
            f"({len(rows)} benchmarks, ±{regress_pct:.0f}% threshold) ==="
        )
        print(f"{'benchmark':42}  {prev:>10}  {cur:>10}  {'Δ':>8}")
        print("-" * 76)
        for name, a, b, pct in rows[:top]:
            flag = (
                " ⚠"
                if pct > regress_pct
                else (" ✓" if pct < -regress_pct else "")
            )
            disp = name if len(name) <= 42 else name[:39] + "..."
            print(
                f"{disp:42}  {fmt_time(a):>10}  {fmt_time(b):>10}  {pct:>+7.1f}%{flag}"
            )
        if len(rows) > top:
            print(
                f"... {len(rows) - top} more (use --top {len(rows)} for all)"
            )
        print(
            f"summary: {regressions} regressed >{regress_pct:.0f}%, "
            f"{improvements} improved >{regress_pct:.0f}%"
        )
    if not any_data:
        print(
            "No release snapshots found on the `benchmarks` branch "
            "(need ≥2 releases). Is origin/benchmarks fetched?"
        )
        return 1
    return 0


def cmd_page(series, out: str, assets: str, top: int) -> int:
    """Write a docs markdown page with per-suite trend plots over releases."""
    import os

    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    os.makedirs(assets, exist_ok=True)
    rel_assets = os.path.relpath(assets, os.path.dirname(out) or ".")
    lines = [
        "# Benchmarks",
        "",
        "Per-release benchmark history, captured automatically on every release",
        "tag by [`benchmark.yml`](https://github.com/doppler-dsp/doppler/blob/"
        "main/.github/workflows/benchmark.yml) on a pinned runner "
        "(ubuntu-24.04, Python 3.12) and stored on the `benchmarks` branch. "
        "Lower is better (mean wall-clock per call).",
        "",
    ]

    produced = False
    for suite in SUITES:
        snaps = series[suite]
        tags = list(snaps)
        if len(tags) < 2:
            continue
        produced = True
        # The N slowest benchmarks in the newest release — the ones worth watching.
        newest = snaps[tags[-1]]
        watch = sorted(newest, key=lambda n: -newest[n])[:top]

        fig, ax = plt.subplots(figsize=(10, 5.5))
        xs = range(len(tags))
        for name in watch:
            ys = [snaps[t].get(name) for t in tags]
            ax.plot(
                xs,
                [y if y else float("nan") for y in ys],
                marker="o",
                ms=3,
                lw=1.2,
                label=name,
            )
        ax.set_xticks(list(xs))
        ax.set_xticklabels(tags, rotation=45, ha="right", fontsize=7)
        ax.set_ylabel("mean time per call (s)")
        ax.set_yscale("log")
        ax.set_title(f"{SUITE_LABEL[suite]} — {len(watch)} slowest benchmarks")
        ax.grid(True, which="both", alpha=0.25)
        ax.legend(
            fontsize=6, ncol=2, loc="upper left", bbox_to_anchor=(1.01, 1.0)
        )
        fig.tight_layout()
        png = os.path.join(assets, f"trend-{suite}.png")
        fig.savefig(png, dpi=110)
        plt.close(fig)

        prev, cur, rows = _delta_rows(tags, snaps)
        lines += [
            f"## {SUITE_LABEL[suite]}",
            "",
            f"![{suite} benchmark trend]({rel_assets}/trend-{suite}.png)",
            "",
            f"### {cur} vs {prev}",
            "",
            f"| benchmark | {prev} | {cur} | Δ |",
            "| --- | ---: | ---: | ---: |",
        ]
        for name, a, b, pct in rows[:top]:
            lines.append(
                f"| `{name}` | {fmt_time(a)} | {fmt_time(b)} | {pct:+.1f}% |"
            )
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
    p.add_argument(
        "--out",
        default="docs/benchmarks.md",
        help="markdown output path (--page mode)",
    )
    p.add_argument(
        "--assets",
        default="docs/assets/bench",
        help="directory for generated plot PNGs (--page mode)",
    )
    p.add_argument(
        "--top",
        type=int,
        default=25,
        help="rows/series to show per suite (default 25)",
    )
    p.add_argument(
        "--threshold",
        type=float,
        default=10.0,
        help="±%% to flag as a regression/improvement (table mode)",
    )
    a = p.parse_args()

    series = load_history()
    if a.page:
        return cmd_page(series, a.out, a.assets, a.top)
    return cmd_table(series, a.top, a.threshold)


if __name__ == "__main__":
    raise SystemExit(main())
