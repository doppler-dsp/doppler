"""Benchmark for AsyncDsssReceiver.

Run: pytest src/doppler/dsss/benchmarks/bench_async_dsss_receiver.py
     --benchmark-only
"""

import numpy as np
import pytest

from doppler.dsss import AsyncDsssReceiver
from doppler.wfm import Gold

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    # A real 1023-chip Gold code, as the pool's benchmark uses. This fixture
    # used a 1-chip code, which spreads nothing and which the receiver now
    # refuses: its Dll's early and late taps would coincide (doppler#2103).
    return AsyncDsssReceiver(
        np.asarray(Gold().generate(1023)).astype(np.uint8),
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
