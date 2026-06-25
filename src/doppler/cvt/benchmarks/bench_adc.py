"""Benchmark for ADC.

Run: pytest src/doppler/cvt/benchmarks/bench_adc.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.cvt import ADC

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return ADC(16, -10.0, 0)


def test_bench_step(benchmark, obj):
    benchmark(obj.step, 1.0)


def test_bench_steps_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.float32)
    benchmark(obj.steps, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
