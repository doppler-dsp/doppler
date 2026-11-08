"""Benchmark for the 1-D detector (Correlator, detect=1).

Run: make bench
"""

import numpy as np
import pytest

from doppler.correlation import Correlator

N = 64
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Correlator(
        np.zeros(N, dtype=np.complex64),
        detect=1,
        dwell=4,
        noise_lo=1,
        noise_hi=N - 2,
        noise_mode="mean",
        threshold=0.0,
        nthreads=1,
    )


def test_bench_push_64k(benchmark, obj):
    # Every dump fires (threshold 0): 65536 samples at n = 64, dwell 4 is
    # 256 detections. Checked, because a push that fills its results stops
    # taking input -- at the old room of 64 this timed 16384 samples while
    # crediting 65536.
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    hits = benchmark(obj.push, x)
    assert len(hits) == BLOCK_64K // (N * 4)
