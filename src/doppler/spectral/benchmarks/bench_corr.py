"""Benchmark for Corr.

Run: pytest src/doppler/spectral/benchmarks/bench_corr.py --benchmark-only
"""
import pytest
import numpy as np

from doppler.spectral import Corr

N         = 64
BLOCK_1K  = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Corr(np.zeros(N, dtype=np.complex64), 4)

def test_bench_execute_1k(benchmark, obj):
    x = np.ones(BLOCK_1K, dtype=np.complex64)
    benchmark(obj.execute, x)

def test_bench_execute_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(obj.execute, x)
