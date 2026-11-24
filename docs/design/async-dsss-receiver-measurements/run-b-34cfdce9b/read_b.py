"""Run (b), read: the pool's loss release on a forced ghost, two rules.

The run (34cfdce9b, a clean tree; see STATUS): validate_async_dsss_pool_ghosts
--emit --forced-s 20 in 16 shards, and the pull-in validator's --telemetry
on the twelve 4 dB draws of step D's run (d-6fc84cc80) whose code flag
changed after the declared lock. Its sections are merged here:
ghosts_forced.csv, ghosts_runs.csv, ghosts_second.csv, ghosts_alone.csv
(header only: every shard that reached the SECOND arm's REQUIRE aborted
there, so no ALONE row was written), code_down_draws.csv (the twelve
draws' rows) and code_down_runs.csv (extract_code_down.py over their
captures, which stay with the run's logs). shards.txt is every shard's
stderr and exit.

Printed:

1. FORCED, per carrier and k: how the shipped pool ended (held, released
   by the ghost rule, freed by the loss release: both flags down for
   lost_confirm_s, 2 s), and how many a code-flag rule (the code flag
   down for 2 s) would have released, read as the ghost's longest
   code-down run reaching 2 s; the parent's (receiver 0's) longest
   code-down and both-down runs; the ghost's last code-up after the seed.
2. The ghost's code-down runs, binned, per carrier, k = 2.
3. The true signal at the floor: the longest code-down run after the
   declared lock in the twelve captures.
4. SECOND, per carrier and B's level: B released by the ghost rule.

Counts and orders rows only.

Usage:  python -I read_b.py
"""

import csv
import math
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
#: The pool's lost_confirm_s in the run (the manifest's default), s.
CONFIRM_S = 2.0
OUTCOME = {"0": "held", "1": "ghost rule", "2": "loss release"}


def rows(name: str) -> list[dict[str, str]]:
    with (HERE / name).open(newline="", encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


def f(x: object) -> float:
    try:
        return float(x)  # type: ignore[arg-type]
    except (TypeError, ValueError):
        return math.nan


def main() -> None:
    forced = rows("ghosts_forced.csv")
    by = defaultdict(list)
    for r in forced:
        by[(r["fc_ghz"], int(r["k"]))].append(r)
    print(
        "| fc / k | draws | shipped: held / ghost rule / loss release | "
        "code-flag rule would release | parent's longest code-down / "
        "both-down s | ghost's last code-up, median / max s | held seeds |"
    )
    print("|---|---|---|---|---|---|---|")
    for key in sorted(by):
        c = by[key]
        out = Counter(OUTCOME[r["outcome"]] for r in c)
        code_rule = sum(
            r["outcome"] != "0" or f(r["code_down1_s"]) >= CONFIRM_S for r in c
        )
        last = sorted(
            f(r["t_code_last_s"])
            for r in c
            if not math.isnan(f(r["t_code_last_s"]))
        )
        held = [r["seed"] for r in c if r["outcome"] == "0"]
        print(
            f"| {key[0]} / {key[1]:+d} | {len(c)} | {out['held']} / "
            f"{out['ghost rule']} / {out['loss release']} | {code_rule} | "
            f"{max(f(r['code_down0_s']) for r in c):.3f} / "
            f"{max(f(r['both_down0_s']) for r in c):.3f} | "
            f"{last[len(last) // 2] if last else math.nan:.2f} / "
            f"{last[-1] if last else math.nan:.2f} | "
            f"{' '.join(held) or '-'} |"
        )

    print("\nthe ghost's code-down runs (receiver 1), k = 2, by length:")
    edges = [0.0, 0.05, 0.2, 0.5, 1.0, 2.0, 5.0, 21.0]
    for fc in ("2.2", "2.8"):
        h = Counter()
        for r in rows("ghosts_runs.csv"):
            if (
                r["fc_ghz"] == fc
                and r["k"] == "2"
                and r["receiver"] == "1"
                and r["both"] == "0"
            ):
                x = f(r["len_s"])
                for lo, hi in zip(edges, edges[1:], strict=False):
                    if lo <= x < hi:
                        h[f"[{lo:g}, {hi:g}) s"] += 1
        print(f"  {fc} GHz: " + ", ".join(f"{k} {v}" for k, v in h.items()))

    print("\nthe true signal at the floor, code-down after the declared lock:")
    worst = defaultdict(float)
    for r in rows("code_down_runs.csv"):
        worst[r["draw"]] = max(worst[r["draw"]], f(r["len_s"]))
    for d in sorted(worst):
        print(f"  {d}: longest {worst[d]:.4f} s")
    print(f"  all twelve: longest {max(worst.values()):.4f} s")

    print("\nSECOND: a distinct B on A's carrier, released by the ghost rule")
    sec = defaultdict(list)
    for r in rows("ghosts_second.csv"):
        sec[(r["fc_ghz"], r["b_dbhz"])].append(r)
    for key in sorted(sec, key=lambda k: (k[0], f(k[1]))):
        c = sec[key]
        rel = [r["seed"] for r in c if r["outcome"] == "1"]
        print(
            f"  {key[0]} GHz, B at {key[1]} dB-Hz: {len(c)} rows, "
            f"released {len(rel)} (seeds {' '.join(rel) or '-'})"
        )


if __name__ == "__main__":
    main()
