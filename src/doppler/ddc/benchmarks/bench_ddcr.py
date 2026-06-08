"""Benchmark for DDCR.

Run: pytest src/doppler/ddc/benchmarks/bench_ddcr.py --benchmark-only
"""

import pytest
import numpy as np

from doppler.ddc import DDCR

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return DDCR(0.0, 0.25)


def test_bench_execute_1k(benchmark, obj):
    x = np.ones(BLOCK_1K, dtype=np.float32)
    benchmark(obj.execute, x)


def test_bench_execute_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.float32)
    benchmark(obj.execute, x)
