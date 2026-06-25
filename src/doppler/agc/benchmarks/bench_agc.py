"""Benchmark for AGC.

Run: pytest src/doppler/agc/benchmarks/bench_agc.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.agc import AGC

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return AGC(0.0, 0.01, 0.05)


def test_bench_step(benchmark, obj):
    benchmark(obj.step, 1.0 + 0.0j)


def test_bench_steps_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(obj.steps, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
