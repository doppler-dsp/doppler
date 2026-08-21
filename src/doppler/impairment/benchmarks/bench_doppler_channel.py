"""Benchmark for DopplerChannel.

Run: pytest src/doppler/impairment/benchmarks/bench_doppler_channel.py
     --benchmark-only
"""

import pytest

from doppler.impairment import DopplerChannel

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return DopplerChannel(1000000.0, 0.0, 0.0, 0.0)
