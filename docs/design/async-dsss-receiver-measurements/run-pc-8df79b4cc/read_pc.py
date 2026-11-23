"""The pool columns run, read: two derived discriminators and k = 0 admitted.

The run (8df79b4cc, a clean tree; see STATUS): validate_async_dsss_pool_ghosts
--emit --forced-s 20 in 16 shards. FORCED seeds a ghost on a 60 dB-Hz
parent's 167-chip sidelobe at k lattice steps and runs 20 s; SECOND seeds a
distinct emitter B on a 70 dB-Hz A's carrier (within 5 Hz) at a drawn code
offset and runs until both rings have been full a window (at most 4 s).
Both sample, every 0.1 s while both receivers track, the second receiver's
Dll prompt amplitude over the first's and their code-phase difference.
ghosts_alone.csv holds a header only: the ALONE arm wrote no row in any
shard (not read here).

Printed:

1. FORCED per carrier and k (the ghosts, the positives): the shipped
   pool's ending; the ghost rule with k = 0 admitted (any_k); the prompt
   ratio's per-draw median, its spread over the draws (the 10th
   percentile of the medians to the 90th) and the per-draw 10-90 width;
   the code-phase difference's spread over the run.
2. SECOND per carrier and B's level (the distinct emitters, the
   negatives): scored draws and draws with no parent; the shipped ghost
   rule's releases; k = 0 admitted; the same ratio and phase columns.
3. Discriminator 1 sample by sample: the share of samples whose prompt
   ratio is within 3 dB of the code's autocorrelation at that sample's
   phase difference, ghosts against distinct emitters.

Counts and orders rows only.

Usage:  python -I read_pc.py
"""

import csv
import math
import statistics as st
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUTCOME = {"-1": "no parent", "0": "held", "1": "ghost rule", "2": "loss"}


def rows(name: str) -> list[dict[str, str]]:
    with (HERE / name).open(newline="", encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


def f(x: object) -> float:
    try:
        return float(x)  # type: ignore[arg-type]
    except (TypeError, ValueError):
        return math.nan


def clean(v: list[float]) -> list[float]:
    return [x for x in v if not math.isnan(x)]


def q(v: list[float], p: float) -> float:
    v = sorted(clean(v))
    return v[min(len(v) - 1, int(p * (len(v) - 1)))] if v else math.nan


def ratio_cols(c: list[dict[str, str]]) -> str:
    med = [f(r["ratio_med_db"]) for r in c]
    width = [f(r["ratio_p90_db"]) - f(r["ratio_p10_db"]) for r in c]
    spread = [f(r["dphi_spread"]) for r in c]
    return (
        f"{q(med, 0.5):.1f} [{q(med, 0.1):.1f}, {q(med, 0.9):.1f}] | "
        f"{q(width, 0.5):.1f} | {q(spread, 0.5):.2f} / {q(spread, 1.0):.2f}"
    )


def main() -> None:
    forced = defaultdict(list)
    for r in rows("ghosts_forced.csv"):
        forced[(r["fc_ghz"], int(r["k"]))].append(r)
    print(
        "FORCED (ghosts): | fc / k | held / ghost rule / loss | k = 0 "
        "admitted takes | prompt ratio median dB [p10, p90 of draws] | "
        "per-draw p10-p90 width dB | dphi spread median / max chips |"
    )
    for key in sorted(forced):
        c = forced[key]
        o = Counter(OUTCOME[r["outcome"]] for r in c)
        print(
            f"| {key[0]} / {key[1]:+d} | {o['held']} / {o['ghost rule']} / "
            f"{o['loss']} | {sum(r['any_k'] == '1' for r in c)} of "
            f"{len(c)} | {ratio_cols(c)} |"
        )

    second = defaultdict(list)
    for r in rows("ghosts_second.csv"):
        second[(r["fc_ghz"], r["b_dbhz"])].append(r)
    print(
        "\nSECOND (distinct B): | fc / B dB-Hz | scored (no parent) | "
        "ghost rule released | k = 0 admitted takes | prompt ratio median "
        "dB [p10, p90 of draws] | per-draw p10-p90 width dB | dphi spread "
        "median / max chips |"
    )
    for key in sorted(second, key=lambda k: (k[0], f(k[1]))):
        c = [r for r in second[key] if r["outcome"] != "-1"]
        orphan = len(second[key]) - len(c)
        print(
            f"| {key[0]} / {key[1]} | {len(c)} ({orphan}) | "
            f"{sum(r['outcome'] == '1' for r in c)} | "
            f"{sum(r['any_k'] == '1' for r in c)} | {ratio_cols(c)} |"
        )

    print("\ndiscriminator 1, sample by sample: |ratio - acf(dphi)| < 3 dB")
    hit = defaultdict(Counter)
    for r in rows("ghosts_ratio.csv"):
        x, a = f(r["ratio_db"]), f(r["acf_db"])
        if math.isnan(x):
            continue
        arm = "ghosts (FORCED)" if r["arm"] == "0" else "distinct (SECOND)"
        hit[(arm, r["fc_ghz"])]["n"] += 1
        hit[(arm, r["fc_ghz"])]["near"] += abs(x - a) < 3.0
    for key in sorted(hit):
        h = hit[key]
        print(
            f"  {key[0]}, {key[1]} GHz: {h['near']} of {h['n']} samples "
            f"({h['near'] / h['n']:.2f})"
        )
    acf = Counter(round(f(r["acf_db"]), 1) for r in rows("ghosts_ratio.csv"))
    print(
        "  the autocorrelation values the samples fell on, dB: "
        + ", ".join(f"{k} ({v})" for k, v in sorted(acf.items()))
    )
    med = [f(r["ratio_med_db"]) for r in rows("ghosts_forced.csv")]
    print(f"  all ghosts' median ratio: {st.median(clean(med)):.1f} dB")


if __name__ == "__main__":
    main()
