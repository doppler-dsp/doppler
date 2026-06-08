"""Benchmark for CIC decimation filter.

Run: pytest src/doppler/resample/benchmarks/bench_cic.py --benchmark-only
"""

import pytest
import numpy as np

from doppler.resample import CIC

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def cic_32():
    return CIC(32)


def test_bench_decimate_1k(benchmark, cic_32):
    x = np.ones(BLOCK_1K, dtype=np.complex64)
    benchmark(cic_32.decimate, x)


def test_bench_decimate_64k(benchmark, cic_32):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(cic_32.decimate, x)


@pytest.mark.parametrize("R", [4, 8, 32, 64, 256])
def test_bench_decimate_params(benchmark, R):
    cic = CIC(R)
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark.name = f"decimate_R{R}"
    benchmark(cic.decimate, x)
