"""Benchmark for FFT.

Run: pytest src/doppler/spectral/benchmarks/bench_fft.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.spectral import FFT

N_1K = 1_024
N_8K = 8_192


@pytest.fixture
def fft_1k():
    return FFT(N_1K, -1, 1)


@pytest.fixture
def fft_8k():
    return FFT(N_8K, -1, 1)


def test_bench_execute_cf64_1k(benchmark, fft_1k):
    x = np.ones(N_1K, dtype=np.complex128)
    benchmark(fft_1k.execute_cf64, x)


def test_bench_execute_cf32_1k(benchmark, fft_1k):
    x = np.ones(N_1K, dtype=np.complex64)
    benchmark(fft_1k.execute_cf32, x)


def test_bench_execute_cf64_8k(benchmark, fft_8k):
    x = np.ones(N_8K, dtype=np.complex128)
    benchmark(fft_8k.execute_cf64, x)


def test_bench_execute_cf32_8k(benchmark, fft_8k):
    x = np.ones(N_8K, dtype=np.complex64)
    benchmark(fft_8k.execute_cf32, x)
