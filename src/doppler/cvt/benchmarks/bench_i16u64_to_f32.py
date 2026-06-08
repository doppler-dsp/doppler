"""Benchmark for I16U64ToF32.

Run: pytest src/doppler/cvt/benchmarks/bench_i16u64_to_f32.py --benchmark-only
"""

import pytest
import numpy as np

from doppler.cvt import I16U64ToF32

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return I16U64ToF32(32768.0)


def test_bench_step(benchmark, obj):
    benchmark(obj.step, 1)


def test_bench_steps_1k(benchmark, obj):
    x = np.ones(BLOCK_1K, dtype=np.uint64)
    benchmark(obj.steps, x)


def test_bench_steps_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.uint64)
    benchmark(obj.steps, x)
