"""Benchmark for NPRMeasure.

Run: pytest src/doppler/measure/benchmarks/bench_nprmeas.py --benchmark-only
"""

import pytest

from doppler.measure import NPRMeasure

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return NPRMeasure(n=8192, fs=1.0, full_scale=1.0)
