"""The S-alone seed rate behind validate_async_dsss_near_far's gate.

Reads ``alone.csv`` beside this file (validate_async_dsss_near_far --emit
--level 0 --trials 2000 --shard i/16 at the head in STATUS) and prints the
valid trials, the rank-1 search's seeds with S alone on the air, the rate
and its Wilson 95% interval. The interval's upper end is the gate's bound
(ALONE_RATE_BOUND), standing in until doppler#3293 derives the effective
cell count. Counts rows only.

Usage:  python -I read_alone.py
"""

import csv
import math
import pathlib

HERE = pathlib.Path(__file__).resolve().parent


def wilson(k: int, n: int, z: float = 1.96) -> tuple[float, float]:
    """Wilson 95% interval of k successes in n."""
    p = k / n
    d = 1 + z * z / n
    c = (p + z * z / (2 * n)) / d
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / d
    return max(0.0, c - h), min(1.0, c + h)


def main() -> None:
    with open(HERE / "alone.csv", newline="") as fh:
        rows = list(csv.DictReader(fh))
    trials = sum(int(r["trials"]) for r in rows)
    valid = sum(int(r["valid"]) for r in rows)
    alone = sum(int(r["alone_sidelobe"]) for r in rows)
    lo, hi = wilson(alone, valid)
    print(
        f"D = 1, 0 dB: {trials} trials, {valid} valid, S alone seeded "
        f"{alone}: {100 * alone / valid:.3f}% "
        f"(Wilson 95% {100 * lo:.3f}% to {100 * hi:.3f}%)"
    )


if __name__ == "__main__":
    main()
