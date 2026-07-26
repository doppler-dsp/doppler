"""Benchmark for MpskReceiverR.

Run: pytest src/doppler/track/benchmarks/bench_mpsk_receiver_r.py \
     --benchmark-only
"""

import pytest

from doppler.track import MpskReceiverR

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return MpskReceiverR(
        4,
        16.0,
        4,
        "iandd",
        0.35,
        8,
        0.01,
        0.707,
        0.01,
        0,
        0.5,
        0.0,
        100,
        0,
        1024,
    )
