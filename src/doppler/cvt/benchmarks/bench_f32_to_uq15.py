"""Benchmark for F32ToUQ15.

Run: pytest src/doppler/cvt/benchmarks/bench_f32_to_uq15.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.cvt import F32ToUQ15

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return F32ToUQ15(32768.0)


def test_bench_step(benchmark, obj):
    benchmark(obj.step, 1.0)


def test_bench_steps_1k(benchmark, obj):
    x = np.ones(BLOCK_1K, dtype=np.float32)
    benchmark(obj.steps, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_1K / benchmark.stats["mean"] / 1e6
        )


def test_bench_steps_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.float32)
    benchmark(obj.steps, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
