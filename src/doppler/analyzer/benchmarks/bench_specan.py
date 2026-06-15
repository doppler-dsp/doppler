"""Benchmark for Specan.

Run: pytest src/doppler/analyzer/benchmarks/bench_specan.py --benchmark-only
"""

import pytest

from doppler.analyzer import Specan

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Specan(0.0, 0.0, 0.0, "kaiser", 0.0, 0.0, 0.0, 1)
