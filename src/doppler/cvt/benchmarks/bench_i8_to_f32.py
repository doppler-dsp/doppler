"""Benchmark for I8ToF32.

Run: pytest src/doppler/cvt/benchmarks/bench_i8_to_f32.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.cvt import I8ToF32

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return I8ToF32(128.0)


def test_bench_step(benchmark, obj):
    benchmark(obj.step, 1)


def test_bench_steps_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.int8)
    benchmark(obj.steps, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
