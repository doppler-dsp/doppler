"""Benchmark for Detector.

Run: pytest src/doppler/spectral/benchmarks/bench_detector.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.spectral import Detector

N = 64
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Detector(
        np.zeros(N, dtype=np.complex64), "mean", 4, 1, N - 2, 0.0, 1
    )


def test_bench_push_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(obj.push, x)
