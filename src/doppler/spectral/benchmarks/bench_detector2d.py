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
    # Every dump fires (threshold 0): 65536 samples at 8 x 8, dwell 4 is
    # 256 detections. Checked, because a push that fills its results stops
    # taking input -- at the old room of 64 this timed 16384 samples while
    # crediting 65536.
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    hits = benchmark(obj.push, x)
    assert len(hits) == BLOCK_64K // (NY * NX * 4)
