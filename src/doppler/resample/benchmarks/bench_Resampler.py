"""Benchmark for Resampler.

Run: pytest src/doppler/resample/benchmarks/bench_Resampler.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.resample import Resampler

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def decim():
    return Resampler(0.5)


@pytest.fixture
def interp():
    return Resampler(2.0)


def test_bench_execute_decim_1k(benchmark, decim):
    x = np.ones(BLOCK_1K, dtype=np.complex64)
    benchmark(decim.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = BLOCK_1K / benchmark.stats["mean"] / 1e6


def test_bench_execute_decim_64k(benchmark, decim):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(decim.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = BLOCK_64K / benchmark.stats["mean"] / 1e6


def test_bench_execute_interp_1k(benchmark, interp):
    x = np.ones(BLOCK_1K, dtype=np.complex64)
    benchmark(interp.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = BLOCK_1K / benchmark.stats["mean"] / 1e6
