"""Benchmark for FFT2D.

Run: pytest src/doppler/spectral/benchmarks/bench_fft2d.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.spectral import FFT2D

NY, NX = 64, 64


@pytest.fixture
def fft2d():
    return FFT2D(NY, NX, -1, 1)


def test_bench_execute_cf64(benchmark, fft2d):
    x = np.ones(NY * NX, dtype=np.complex128)
    benchmark(fft2d.execute_cf64, x)


def test_bench_execute_cf32(benchmark, fft2d):
    x = np.ones(NY * NX, dtype=np.complex64)
    benchmark(fft2d.execute_cf32, x)
