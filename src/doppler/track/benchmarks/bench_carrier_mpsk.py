"""Benchmark for CarrierMpsk.

Run: pytest src/doppler/track/benchmarks/bench_carrier_mpsk.py --benchmark-only
"""

import pytest

from doppler.track import CarrierMpsk

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return CarrierMpsk(0.05, 0.707, 0.0, 64, 0.0, 4)
