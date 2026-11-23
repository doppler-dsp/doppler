"""The data-copy run, read: a ghost's output against its parent's.

The run (145281207, a clean tree; see STATUS): validate_async_dsss_pool_ghosts
--emit --forced-s 20 --second-s 20 in 16 shards. FORCED seeds a ghost on
a 60 dB-Hz parent's 167-chip sidelobe at k lattice steps; SECOND seeds a
distinct emitter B, its own payload, on a 70 dB-Hz A's carrier (within
5 Hz) at a drawn code offset. Both run 20 s past the seed. The per-draw
rows are ghosts_forced.csv and ghosts_second.csv (the run's other
sections are not kept here); shards.txt is every shard's stderr and exit.

The data-copy test (145281207's message): the second receiver's soft
symbols against the first's over every push both track, best over a
+-32 symbol lag; rho_coh one coherent sum, rho_nc the mean of 100-symbol
blocks' normalized sums. Nulls with independent data: rho_coh about
1/sqrt(n), rho_nc about sqrt(pi)/(2 sqrt(100)) = 0.089.

Printed:

1. FORCED per carrier and k: the shipped ending, rho_nc and rho_coh
   (median [min, max] over the draws), the symbols read, and the phase
   difference's spread per 2 s window (median over the draws).
2. SECOND per carrier, B's level and B's lag class (the code's
   autocorrelation at B's first phase difference: 63, -65 or -1 of
   1023): the draws, the shipped ghost rule's releases, rho_nc, B's error
   rate against its payload, and the window spread.
3. The refutations named before the run: a ghost whose rho_nc is within
   0.03 of the null, and a B that decodes (error rate under 1e-2) with
   rho_nc over 0.2.

Counts and orders rows only.

Usage:  python -I read_rho.py
"""

import csv
import math
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUTCOME = {"-1": "no parent", "0": "held", "1": "ghost rule", "2": "loss"}
#: rho_nc's null with independent data: sqrt(pi) / (2 sqrt(100)).
NULL_NC = math.sqrt(math.pi) / (2.0 * math.sqrt(100.0))


def rows(name: str) -> list[dict[str, str]]:
    with (HERE / name).open(newline="", encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


def f(x: object) -> float:
    try:
        return float(x)  # type: ignore[arg-type]
    except (TypeError, ValueError):
        return math.nan


def mmm(v: list[float]) -> str:
    v = sorted(x for x in v if not math.isnan(x))
    if not v:
        return "-"
    return f"{v[(len(v) - 1) // 2]:.3f} [{v[0]:.3f}, {v[-1]:.3f}]"


def med(v: list[float]) -> float:
    v = sorted(x for x in v if not math.isnan(x))
    return v[(len(v) - 1) // 2] if v else math.nan


def main() -> None:
    print(f"rho_nc null: {NULL_NC:.3f}")
    forced = defaultdict(list)
    for r in rows("ghosts_forced.csv"):
        forced[(r["fc_ghz"], int(r["k"]))].append(r)
    print(
        "\nFORCED (ghosts): | fc / k | held / ghost rule / loss | rho_nc "
        "median [min, max] | rho_coh median [min, max] | symbols read, "
        "median | window spread, median chips |"
    )
    for key in sorted(forced):
        c = forced[key]
        o = Counter(OUTCOME[r["outcome"]] for r in c)
        print(
            f"| {key[0]} / {key[1]:+d} | {o['held']} / {o['ghost rule']} / "
            f"{o['loss']} | {mmm([f(r['rho_nc']) for r in c])} | "
            f"{mmm([f(r['rho_coh']) for r in c])} | "
            f"{med([f(r['n_rho']) for r in c]):.0f} | "
            f"{med([f(r['dphi_win_med']) for r in c]):.2f} |"
        )

    second = defaultdict(list)
    for r in rows("ghosts_second.csv"):
        if r["outcome"] == "-1":
            continue
        second[(r["fc_ghz"], f(r["b_dbhz"]), r["lag_r"])].append(r)
    print(
        "\nSECOND (distinct B): | fc / B dB-Hz / lag class | draws | ghost "
        "rule released | rho_nc median [min, max] | B's error rate median "
        "[min, max] | window spread, median chips |"
    )
    for key in sorted(second):
        c = second[key]
        print(
            f"| {key[0]} / {key[1]:.1f} / {key[2]} | {len(c)} | "
            f"{sum(r['outcome'] == '1' for r in c)} | "
            f"{mmm([f(r['rho_nc']) for r in c])} | "
            f"{mmm([f(r['b_ber']) for r in c])} | "
            f"{med([f(r['dphi_win_med']) for r in c]):.2f} |"
        )

    print("\nrefutations named before the run:")
    near = [
        r
        for r in rows("ghosts_forced.csv")
        if abs(f(r["rho_nc"]) - NULL_NC) < 0.03
    ]
    print(f"  ghosts with rho_nc within 0.03 of the null: {len(near)}")
    for r in near:
        print(
            f"    {r['fc_ghz']} GHz k {r['k']} seed {r['seed']}: rho_nc "
            f"{r['rho_nc']}, symbols {r['n_rho']}, outcome "
            f"{OUTCOME[r['outcome']]}"
        )
    clean = [
        r
        for r in rows("ghosts_second.csv")
        if r["outcome"] != "-1"
        and f(r["b_ber"]) < 1e-2
        and f(r["rho_nc"]) > 0.2
    ]
    print(f"  B decoding under 1e-2 with rho_nc over 0.2: {len(clean)}")
    for r in clean:
        print(
            f"    {r['fc_ghz']} GHz B {r['b_dbhz']} seed {r['seed']}: "
            f"rho_nc {r['rho_nc']}, error rate {r['b_ber']}, lag class "
            f"{r['lag_r']}"
        )


if __name__ == "__main__":
    main()
