"""Benchmark for F32ToI32.

Run: pytest src/doppler/cvt/benchmarks/bench_f32_to_i32.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.cvt import F32ToI32

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return F32ToI32(scale=2147483648.0)


def test_bench_step(benchmark, obj):
    benchmark(obj.step, 1.0)


def test_bench_steps_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.float32)
    benchmark(obj.steps, x)
