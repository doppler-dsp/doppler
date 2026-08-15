"""Benchmark for FrameMeter.

Run: pytest src/doppler/ber/benchmarks/bench_frame_meter.py --benchmark-only
"""

import pytest

from doppler.ber import FrameMeter

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return FrameMeter(target_errors=200, conf=0.99)
