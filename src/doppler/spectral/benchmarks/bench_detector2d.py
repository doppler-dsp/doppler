"""Benchmark for CorrDetector2D.

Run: pytest src/doppler/spectral/benchmarks/bench_detector2d.py
     --benchmark-only
"""

import numpy as np
import pytest

from doppler.spectral import CorrDetector2D

NY = 8
NX = 8
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return CorrDetector2D(
        np.zeros((NY, NX), dtype=np.complex64),
        dwell=4,
        noise_lo=1,
        noise_hi=NY * NX - 2,
        noise_mode="mean",
        threshold=0.0,
        nthreads=1,
    )


def test_bench_push_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(obj.push, x)
