"""Benchmark for ToneMeasure.

Run: pytest src/doppler/measure/benchmarks/bench_tonemeas.py --benchmark-only
"""

import pytest

from doppler.measure import ToneMeasure

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return ToneMeasure("kaiser", 8192, 1.0, 12.0, 2, 8, 1.0, 0)
