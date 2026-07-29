"""Benchmark for IMDMeasure.

Run: pytest src/doppler/measure/benchmarks/bench_imdmeas.py --benchmark-only
"""

import pytest

from doppler.measure import IMDMeasure

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return IMDMeasure(n=8192, fs=1.0, full_scale=1.0)
