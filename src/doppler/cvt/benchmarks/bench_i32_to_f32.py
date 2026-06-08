"""Benchmark for I32ToF32.

Run: pytest src/doppler/cvt/benchmarks/bench_i32_to_f32.py --benchmark-only
"""

import pytest
import numpy as np

from doppler.cvt import I32ToF32

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return I32ToF32(2147483648.0)


def test_bench_step(benchmark, obj):
    benchmark(obj.step, 1)


def test_bench_steps_1k(benchmark, obj):
    x = np.ones(BLOCK_1K, dtype=np.int32)
    benchmark(obj.steps, x)


def test_bench_steps_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.int32)
    benchmark(obj.steps, x)
