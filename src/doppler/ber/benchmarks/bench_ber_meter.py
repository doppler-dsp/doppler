"""Benchmark for BerMeter.

Run: pytest src/doppler/ber/benchmarks/bench_ber_meter.py --benchmark-only
"""

import pytest

from doppler.ber import BerMeter

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return BerMeter(4, 200, 0.99)
