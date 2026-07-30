"""Benchmark for Capture.

Run standalone:  python src/iqtools/capture/benchmarks/bench_capture.py
Or via make:     make bench
"""

import time

from iqtools.capture import Capture

REPS = 1_000
BLOCK_1K = 1_024
BLOCK_64K = 65_536


def _bench(label: str, fn, *args, reps: int = REPS) -> float:
    for _ in range(max(1, reps // 10)):  # warmup
        fn(*args)
    t0 = time.perf_counter()
    for _ in range(reps):
        fn(*args)
    return (time.perf_counter() - t0) / reps


def main() -> None:
    Capture(...)
    print("capture")


if __name__ == "__main__":
    main()
