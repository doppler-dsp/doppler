"""Benchmark for CIC decimation filter.

Run: pytest src/doppler/resample/benchmarks/bench_cic.py --benchmark-only
"""
import pytest
import numpy as np

from doppler.resample import CIC

BLOCK_1K  = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def cic_32_4():
    return CIC(32, 4, 1)


def test_bench_decimate_1k(benchmark, cic_32_4):
    x = np.ones(BLOCK_1K, dtype=np.complex64)
    benchmark(cic_32_4.decimate, x)


def test_bench_decimate_64k(benchmark, cic_32_4):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(cic_32_4.decimate, x)


@pytest.mark.parametrize("R,N", [(4,2), (8,3), (32,4), (64,4), (256,4)])
def test_bench_decimate_params(benchmark, R, N):
    cic = CIC(R, N, 1)
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark.name = f"decimate_R{R}_N{N}"
    benchmark(cic.decimate, x)
