"""Benchmark for FIR.

Run: pytest src/doppler/filter/benchmarks/bench_fir.py --benchmark-only
"""

import pytest
import numpy as np
from scipy.signal import firwin

from doppler.filter import FIR

BLOCK_1K = 1_024
BLOCK_20K = 20_480
BLOCK_400K = 409_600
BLOCK_800K = 819_200

H = firwin(63, 0.2).astype(np.float32)


@pytest.fixture
def fir():
    return FIR(H)


@pytest.mark.parametrize("n", [BLOCK_1K, BLOCK_20K, BLOCK_400K, BLOCK_800K])
def test_bench_execute(benchmark, fir, n):
    x = np.ones(n, dtype=np.complex64)
    benchmark(fir.execute, x)
