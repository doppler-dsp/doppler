"""Benchmark for DsssReceiver.

Run: pytest src/doppler/dsss/benchmarks/bench_dsss_receiver.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.dsss import DsssReceiver

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return DsssReceiver(
        np.zeros(1, dtype=np.uint8),
        1000000.0,
        1000.0,
        2,
        2,
        55.0,
        1e-3,
        0.9,
        100.0,
        16,
        8,
        4,
        8,
        0,
    )
