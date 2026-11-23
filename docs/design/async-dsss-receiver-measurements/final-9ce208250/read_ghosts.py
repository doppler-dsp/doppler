"""The release's final ghost run, read: outcomes by carrier, k and level.

Reads ``ghosts_forced.csv`` and ``ghosts_second.csv`` beside this file (the
run of 9ce208250; validate_async_dsss_pool_ghosts --emit --forced-s 20
--second-s 20, see STATUS) and prints:

1. FORCED, per carrier and k: held to the end (outcome 0), released by the
   ghost rule (1), freed by the loss release (2), and the held draws' seeds
   with their longest both-flags-down run (s, against lost_confirm_s 2.0)
   and their carrier's offset from A's at the end; the Wilson 95% interval
   of the held fraction;
2. every rule release's end offset from A, Hz (a whole nonzero number of
   Doppler ambiguity intervals: 1,571 Hz at 2.2 GHz, 2,000 at 2.8);
3. SECOND, per carrier and B's level: B held, released, its parent never
   tracking A with code lock (outcome -1), and the counterfactual (k = 0
   admitted) releases.

Counts and orders rows only.

Usage:  python -I read_ghosts.py
"""

import collections
import csv
import math
import pathlib

HERE = pathlib.Path(__file__).resolve().parent


def read_csv(name: str) -> list[dict[str, str]]:
    """The named CSV beside this file, as dicts."""
    with open(HERE / name, newline="") as fh:
        return list(csv.DictReader(fh))


def wilson(k: int, n: int, z: float = 1.96) -> tuple[float, float]:
    """Wilson 95% interval of k successes in n."""
    p = k / n
    d = 1 + z * z / n
    c = (p + z * z / (2 * n)) / d
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / d
    return max(0.0, c - h), min(1.0, c + h)


def main() -> None:
    forced = read_csv("ghosts_forced.csv")
    print(
        "| fc | k | held | released | freed | held seeds (both-down s, Hz)"
        " | held 95% |"
    )
    print("|---|---|---|---|---|---|---|")
    for fc in ("2.2", "2.8"):
        for k in range(-2, 3):
            rows = [
                x for x in forced if x["fc_ghz"] == fc and int(x["k"]) == k
            ]
            n = collections.Counter(x["outcome"] for x in rows)
            held = [x for x in rows if x["outcome"] == "0"]
            lo, hi = wilson(len(held), len(rows))
            seeds = ", ".join(
                f"{x['seed']} ({x['both_down1_s']}, {x['df_end_hz']})"
                for x in sorted(held, key=lambda x: int(x["seed"]))
            )
            print(
                f"| {fc} | {k} | {n['0']} | {n['1']} | {n['2']} | "
                f"{seeds} | {lo:.2f}-{hi:.2f} |"
            )
    rel = sorted(
        round(float(x["df_end_hz"])) for x in forced if x["outcome"] == "1"
    )
    print(f"\nrule releases, end offset from A (Hz): {rel}\n")
    second = read_csv("ghosts_second.csv")
    print("| fc | B dB-Hz | held | released | no parent | k = 0 admitted |")
    print("|---|---|---|---|---|---|")
    for fc in ("2.2", "2.8"):
        for b in ("38.8", "42.0", "44.0", "46.0", "50.0"):
            rows = [
                x for x in second if x["fc_ghz"] == fc and x["b_dbhz"] == b
            ]
            n = collections.Counter(x["outcome"] for x in rows)
            ak = sum(x["any_k"] == "1" for x in rows)
            print(f"| {fc} | {b} | {n['0']} | {n['1']} | {n['-1']} | {ak} |")


if __name__ == "__main__":
    main()
