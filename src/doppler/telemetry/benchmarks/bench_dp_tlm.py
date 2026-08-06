"""Benchmark for Telemetry.

Run: pytest src/doppler/telemetry/benchmarks/bench_dp_tlm.py --benchmark-only
"""

import pytest

from doppler.telemetry import Telemetry

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Telemetry(ring_records=16384)
