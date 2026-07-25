"""Benchmark for RateSync.

Run: pytest src/doppler/track/benchmarks/bench_ratesync.py --benchmark-only
"""

import pytest

from doppler.track import RateSync

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return RateSync(4.0, "rrc", 0.35, 8, 2, 1024, 0.01, 0.707, "gardner")
