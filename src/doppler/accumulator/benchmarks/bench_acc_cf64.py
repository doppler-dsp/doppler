"""Benchmark for AccCf64.

Run: pytest src/doppler/accumulator/benchmarks/ --benchmark-only
"""

import numpy as np
import pytest

from doppler.accumulator import AccCf64


@pytest.fixture
def obj():
    return AccCf64(0j)


@pytest.mark.parametrize("n", [1_024, 20_480, 409_600, 819_200])
def test_bench_steps(benchmark, obj, n):
    x = np.ones(n, dtype=np.complex128)
    benchmark(obj.steps, x)
    if benchmark.stats:
        benchmark.extra_info["GSa_s"] = n / benchmark.stats["mean"] / 1e9
