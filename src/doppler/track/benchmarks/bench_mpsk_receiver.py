"""Benchmark for MpskReceiver.

Run: pytest src/doppler/track/benchmarks/bench_mpsk_receiver.py
"""

import pytest

from doppler.track import MpskReceiver

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return MpskReceiver(
        "iandd", 4, 8, 4, 0.35, 8, 0.01, 0.707, 0.01, 0, 0.5, 0.0, 100, 0
    )
