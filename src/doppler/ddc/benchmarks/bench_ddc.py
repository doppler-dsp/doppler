"""Benchmark for DDC.

Run: pytest src/doppler/ddc/benchmarks/bench_ddc.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.ddc import DDC

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return DDC(0.0, 0.25)


def test_bench_execute_1k(benchmark, obj):
    x = np.ones(BLOCK_1K, dtype=np.complex64)
    benchmark(obj.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_1K / benchmark.stats["mean"] / 1e6
        )


def test_bench_execute_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(obj.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
