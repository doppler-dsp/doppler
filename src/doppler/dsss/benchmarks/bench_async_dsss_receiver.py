"""Benchmark for AsyncDsssReceiver.

Run: pytest src/doppler/dsss/benchmarks/bench_async_dsss_receiver.py
     --benchmark-only
"""

import numpy as np
import pytest

from doppler.dsss import AsyncDsssReceiver

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return AsyncDsssReceiver(
        np.zeros(1, dtype=np.uint8),
        1000000.0,
        1000.0,
        2,
        2,
        55.0,
        1e-3,
        0.9,
        100.0,
        4,
        8,
        0,
        0.5,
        4,
        14.0,
        64,
        8,
        True,
        100000,
    )
