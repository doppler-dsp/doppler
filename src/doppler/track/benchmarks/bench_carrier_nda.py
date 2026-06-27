"""Benchmark for CarrierNda.

Run: pytest src/doppler/track/benchmarks/bench_carrier_nda.py --benchmark-only
"""

import pytest

from doppler.track import CarrierNda

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return CarrierNda(0.01, 0.707, 0.0, 8, 4, 4)
