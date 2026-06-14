"""Benchmark for NPRMeasure.

Run: pytest src/doppler/measure/benchmarks/bench_nprmeas.py --benchmark-only
"""

import pytest

from doppler.measure import NPRMeasure

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return NPRMeasure("kaiser", 8192, 1.0, 12.0, 2, 1.0)
