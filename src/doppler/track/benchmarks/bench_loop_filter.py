"""Benchmark for LoopFilter.

Run: pytest src/doppler/track/benchmarks/bench_loop_filter.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.track import LoopFilter

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return LoopFilter(0.01, 0.707, 1.0)


def test_bench_step(benchmark, obj):
    benchmark(obj.step, 1.0)


def test_bench_steps_1k(benchmark, obj):
    x = np.ones(BLOCK_1K, dtype=np.float64)
    benchmark(obj.steps, x)


def test_bench_steps_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.float64)
    benchmark(obj.steps, x)
