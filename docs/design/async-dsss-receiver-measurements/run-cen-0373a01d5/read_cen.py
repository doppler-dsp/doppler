"""The centered data-copy run, read in #2529's form: caught and taken.

The run (0373a01d5, a clean tree; see STATUS): validate_async_dsss_pool_ghosts
--emit --forced-s 20 --second-s 20 in 16 shards; its per-draw rows are
ghosts_forced.csv and ghosts_second.csv, shards.txt its stderr. The
centered test (c43e439ba's message): each block less its mean before the
normalized |sum| (rc100, rc25), the null measured in the same draw five
whole frames off (nl100, nl25, its block sd ns25), the threshold at 25
symbols thr25 = nl25 + Q_inv(1e-3 / 65) ns25 / sqrt(blocks), and t_cross
the first time the running mean clears it at its own count.

A draw is TAKEN when rc25 clears thr25 (both from the same draw).

Printed:

1. The null against its prediction (0.13 at 100, 0.19 at 25 for this
   frame), over every draw that measured one.
2. Ghosts (FORCED, the positives), per carrier and k: caught, the
   latency to the first crossing (median, worst), and the shipped pool's
   ending beside it (a 2.2 GHz ghost the loss release frees costs nothing
   if missed).
3. Distinct B (SECOND, the negatives), per carrier, level and lag class:
   taken, split into taken-and-undecodable (its error rate over 0.1) and
   taken-though-decoding (under 1e-2: the only real false release).
4. The case named before the run: B at 50 dB-Hz on a +63 / -65 lag.

Counts and orders rows only.

Usage:  python -I read_cen.py
"""

import csv
import math
from collections import defaultdict
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


def med(v: list[float]) -> float:
    v = sorted(x for x in v if not math.isnan(x))
    return v[(len(v) - 1) // 2] if v else math.nan


def taken(r: dict[str, str]) -> bool:
    return f(r["rc25"]) > f(r["thr25"])


def main() -> None:
    fr = rows("ghosts_forced.csv")
    sr = [r for r in rows("ghosts_second.csv") if r["outcome"] != "-1"]
    both = fr + sr
    print(
        "the null, every draw that measured one: nl100 median "
        f"{med([f(r['nl100']) for r in both]):.3f} (predicted 0.13), nl25 "
        f"{med([f(r['nl25']) for r in both]):.3f} (predicted 0.19), ns25 "
        f"{med([f(r['ns25']) for r in both]):.3f}; thr25 median "
        f"{med([f(r['thr25']) for r in both]):.3f}; draws with no null "
        f"(too short): {sum(math.isnan(f(r['nl25'])) for r in both)}"
    )

    by = defaultdict(list)
    for r in fr:
        by[(r["fc_ghz"], int(r["k"]))].append(r)
    print(
        "\nGHOSTS: | fc / k | caught | latency median / worst s | rc25 "
        "median | thr25 median | shipped: held / ghost / loss | caught "
        "among the held |"
    )
    for key in sorted(by):
        c = by[key]
        lat = [f(r["t_cross_s"]) for r in c if taken(r)]
        held = [r for r in c if r["outcome"] == "0"]
        print(
            f"| {key[0]} / {key[1]:+d} | {sum(taken(r) for r in c)} of "
            f"{len(c)} | {med(lat):.2f} / "
            f"{max(lat) if lat else math.nan:.2f} | "
            f"{med([f(r['rc25']) for r in c]):.3f} | "
            f"{med([f(r['thr25']) for r in c]):.3f} | "
            f"{sum(r['outcome'] == '0' for r in c)} / "
            f"{sum(r['outcome'] == '1' for r in c)} / "
            f"{sum(r['outcome'] == '2' for r in c)} | "
            f"{sum(taken(r) for r in held)} of {len(held)} |"
        )

    sb = defaultdict(list)
    for r in sr:
        sb[(r["fc_ghz"], f(r["b_dbhz"]), r["lag_r"])].append(r)
    print(
        "\nDISTINCT B: | fc / B dB-Hz / lag class | draws | taken | taken "
        "and undecodable (BER > 0.1) | taken though decoding (BER < 1e-2) | "
        "rc25 median | thr25 median | BER median |"
    )
    for key in sorted(sb):
        c = sb[key]
        t = [r for r in c if taken(r)]
        print(
            f"| {key[0]} / {key[1]:.1f} / {key[2]} | {len(c)} | {len(t)} | "
            f"{sum(f(r['b_ber']) > 0.1 for r in t)} | "
            f"{sum(f(r['b_ber']) < 1e-2 for r in t)} | "
            f"{med([f(r['rc25']) for r in c]):.3f} | "
            f"{med([f(r['thr25']) for r in c]):.3f} | "
            f"{med([f(r['b_ber']) for r in c]):.4f} |"
        )
    false = [r for r in sr if taken(r) and f(r["b_ber"]) < 1e-2]
    print(
        f"\nfalse releases (taken though decoding): {len(false)} of "
        f"{len(sr)} distinct B"
    )
    print("the named case, B at 50 dB-Hz on a +63 / -65 lag:")
    for r in sr:
        if f(r["b_dbhz"]) == 50.0 and r["lag_r"] in ("63", "-65"):
            print(
                f"  {r['fc_ghz']} GHz seed {r['seed']} lag {r['lag_r']}: rc25 "
                f"{r['rc25']} thr25 {r['thr25']} taken {taken(r)} BER "
                f"{r['b_ber']}"
            )


if __name__ == "__main__":
    main()
