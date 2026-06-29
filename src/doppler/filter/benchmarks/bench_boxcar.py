"""Benchmark for MovingAverage.

Run: pytest src/doppler/filter/benchmarks/bench_boxcar.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.filter import MovingAverage

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return MovingAverage(4, 1.0)


def test_bench_step(benchmark, obj):
    benchmark(obj.step, 1.0 + 0.0j)


def test_bench_steps_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(obj.steps, x)
