"""Benchmark for AccF32.

Run: pytest src/doppler/accumulator/benchmarks/ --benchmark-only
"""

import numpy as np
import pytest

from doppler.accumulator import AccF32


@pytest.fixture
def obj():
    return AccF32(0.0)


@pytest.mark.parametrize("n", [1_024, 20_480, 409_600, 819_200])
def test_bench_steps(benchmark, obj, n):
    x = np.ones(n, dtype=np.float32)
    benchmark(obj.steps, x)
    if benchmark.stats:
        benchmark.extra_info["GSa_s"] = n / benchmark.stats["mean"] / 1e9
