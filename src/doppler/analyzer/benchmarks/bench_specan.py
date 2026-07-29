"""Benchmark for Specan.

Run: pytest src/doppler/analyzer/benchmarks/bench_specan.py --benchmark-only
"""

import pytest

from doppler.analyzer import Specan

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Specan(fs=2.048e6, span=200e3, rbw=500.0, window="kaiser", navg=1)
