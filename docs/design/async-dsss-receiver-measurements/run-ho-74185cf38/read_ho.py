"""The hand-over read of the ghost rule's releases of a distinct B.

Two runs of validate_async_dsss_pool_ghosts --handover --seeds 4,5
--draws 5 --second-s 4 in 2 shards (see each STATUS): handover.csv at
74185cf38, a clean tree, and base/handover.csv at b1b1090d5
(1ba73e06d's parent, the receiver before the fine hand-over) with
74185cf38's validator file checked out over it (dirty=1: that one file).
SECOND seeds receiver 1 at B's TRUE code phase and carrier, a 70 dB-Hz A
on the same carrier (within 5 Hz), and runs 4 s past the seed. Per draw:
B's level and lag class, the hand-over source (1 the seed, 2 the refine,
3 the fine estimate), the handed-over Doppler less B's truth (Hz and in
lattice steps, chip_rate / (2 code_len)), the outcome (1: released by the
ghost rule; -1: A's receiver never locked, not scored) and receiver 1's
code-lock share.

Printed: one row per draw, both runs side by side, then the count of
hand-overs a whole lattice step off B and of releases in each run.

Counts and orders rows only.

Usage:  python -I read_ho.py
"""

import csv
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = {"-1": "not scored", "0": "held", "1": "RELEASED", "2": "lost"}


def rows(path: Path) -> dict[tuple[str, str, str], dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as fh:
        return {
            (r["fc_ghz"], r["b_dbhz"], r["seed"]): r
            for r in csv.DictReader(fh)
        }


#: The code period at each carrier, s (415.1-B-1 as the scene sets it:
#: 11 MSa/s, 3500 and 2750 samples a period).
PERIOD_S = {"2.2": 3500 / 11e6, "2.8": 2750 / 11e6}


def lat(r: dict[str, str], fc: str) -> float:
    """The handed-over error in lattice steps, 1 / (2 T_period) each."""
    return float(r["ho_b_hz"]) * 2.0 * PERIOD_S[fc]


def main() -> None:
    now = rows(HERE / "handover.csv")
    base = rows(HERE / "base" / "handover.csv")
    print(
        "| fc / B dB-Hz / seed | lag | before: src, ho off B Hz, outcome, "
        "code lock | fine (74185cf38): src, ho off B Hz (lattice steps), "
        "outcome, code lock |"
    )
    whole = {"base": 0, "now": 0}
    rel = {"base": 0, "now": 0}
    for key in sorted(now, key=lambda k: (k[0], float(k[1]), int(k[2]))):
        n, b = now[key], base[key]
        if n["outcome"] == "-1":
            print(f"| {' / '.join(key)} | - | not scored | not scored |")
            continue
        for tag, r in (("base", b), ("now", n)):
            whole[tag] += abs(lat(r, key[0])) > 0.5
            rel[tag] += r["outcome"] == "1"
        print(
            f"| {' / '.join(key)} | {n['lag_r']} | {b['ho_src']}, "
            f"{float(b['ho_b_hz']):+.1f}, {OUT[b['outcome']]}, "
            f"{b['code_frac']} | {n['ho_src']}, {float(n['ho_b_hz']):+.1f} "
            f"({lat(n, key[0]):+.2f}), {OUT[n['outcome']]}, "
            f"{n['code_frac']} |"
        )
    scored = sum(r["outcome"] != "-1" for r in now.values())
    print(
        f"\nof {scored} scored draws: handed over a whole lattice step or "
        f"more off B: before {whole['base']}, fine {whole['now']}; released "
        f"by the ghost rule: before {rel['base']}, fine {rel['now']}"
    )


if __name__ == "__main__":
    main()
