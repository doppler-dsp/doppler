"""Benchmark for I16ToF32.

Run: pytest src/doppler/cvt/benchmarks/bench_i16_to_f32.py --benchmark-only
"""

import pytest
import numpy as np

from doppler.cvt import I16ToF32

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return I16ToF32(32768.0)


def test_bench_step(benchmark, obj):
    benchmark(obj.step, 1)


def test_bench_steps_1k(benchmark, obj):
    x = np.ones(BLOCK_1K, dtype=np.int16)
    benchmark(obj.steps, x)


def test_bench_steps_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.int16)
    benchmark(obj.steps, x)
