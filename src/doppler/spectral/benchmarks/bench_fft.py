"""Benchmark for FFT.

Run: pytest src/doppler/spectral/benchmarks/bench_fft.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.spectral import FFT

N_8K = 8_192


@pytest.fixture
def fft_8k():
    return FFT(N_8K, -1, 1)


def test_bench_execute_cf64_8k(benchmark, fft_8k):
    x = np.ones(N_8K, dtype=np.complex128)
    benchmark(fft_8k.execute_cf64, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = N_8K / benchmark.stats["mean"] / 1e6


def test_bench_execute_cf32_8k(benchmark, fft_8k):
    x = np.ones(N_8K, dtype=np.complex64)
    benchmark(fft_8k.execute_cf32, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = N_8K / benchmark.stats["mean"] / 1e6
