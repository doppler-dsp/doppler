"""Benchmark for PSD.

Run: pytest src/doppler/spectral/benchmarks/bench_psd.py --benchmark-only
"""

import pytest

from doppler.spectral import PSD

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return PSD("hann", "mean", 1024, 1.0, 0.0, 0.1)
