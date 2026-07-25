"""Benchmark for RrcSync.

Run: pytest src/doppler/track/benchmarks/bench_rrcsync.py --benchmark-only
"""

import pytest

from doppler.track import RrcSync

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return RrcSync(4.0, 0.35, 8, 1024, 0.005, 0.707, "gardner")
