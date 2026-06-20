"""Benchmark for HalfbandDecimator.

Run: pytest src/doppler/resample/benchmarks/bench_HalfbandDecimator.py
     --benchmark-only
"""

import numpy as np
import pytest

from doppler.resample import HalfbandDecimator, _halfband_bank

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture(scope="module")
def hb_fir():
    bank = _halfband_bank(60.0, 0.4, 0.6)
    centre = bank.shape[1] // 2
    row = 0 if abs(float(bank[0, centre])) < abs(float(bank[1, centre])) else 1
    return np.ascontiguousarray(bank[row])


@pytest.fixture
def obj(hb_fir):
    return HalfbandDecimator(hb_fir)


def test_bench_execute_1k(benchmark, obj):
    x = np.ones(BLOCK_1K, dtype=np.complex64)
    benchmark(obj.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_1K / benchmark.stats["mean"] / 1e6
        )


def test_bench_execute_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(obj.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
