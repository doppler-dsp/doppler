"""Benchmark for IMDMeasure.

Run: pytest src/doppler/measure/benchmarks/bench_imdmeas.py --benchmark-only
"""

import pytest

from doppler.measure import IMDMeasure

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return IMDMeasure("kaiser", 8192, 1.0, 12.0, 2, 1.0)
