"""Benchmark for BurstAcquisition.

Run: pytest src/doppler/dsss/benchmarks/bench_burst_acq.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.dsss import BurstAcquisition

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return BurstAcquisition(
        np.zeros(1, dtype=np.uint8),
        1,
        4,
        1000000.0,
        50.0,
        0.0,
        1e-3,
        0.9,
        "mean",
    )
