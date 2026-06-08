"""Benchmark for Detector2D.

Run: pytest src/doppler/spectral/benchmarks/bench_detector2d.py --benchmark-only
"""

import pytest
import numpy as np

from doppler.spectral import Detector2D

NY = 8
NX = 8
BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Detector2D(
        np.zeros((NY, NX), dtype=np.complex64),
        "mean",
        4,
        1,
        NY * NX - 2,
        0.0,
        1,
    )


def test_bench_push_1k(benchmark, obj):
    x = np.ones(BLOCK_1K, dtype=np.complex64)
    benchmark(obj.push, x)


def test_bench_push_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(obj.push, x)
