"""Benchmark for Welch.

Run: pytest src/doppler/spectral/benchmarks/bench_welch.py --benchmark-only
"""

import pytest

from doppler.spectral import Welch

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Welch("hann", "mean", 1024, 1.0, 0.0, 0.1)
