"""Benchmark for Farrow.

Run: pytest src/doppler/resample/benchmarks/bench_farrow.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.resample import Farrow

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Farrow(order="cubic")


@pytest.fixture
def rx():
    n = BLOCK_64K
    return np.exp(2j * np.pi * 0.05 * np.arange(n)).astype(np.complex64)


def test_bench_delay_64k(benchmark, obj, rx):
    benchmark(obj.delay, rx, 0.5)
    if benchmark.stats:
        obj.reset()
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
