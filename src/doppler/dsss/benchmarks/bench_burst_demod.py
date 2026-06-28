"""Benchmark for BurstDemod.

Run: pytest src/doppler/dsss/benchmarks/bench_burst_demod.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.dsss import BurstDemod

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return BurstDemod(np.zeros(1, dtype=np.uint8), 4, 1.0e6, 0.0, 0.0, 0, 10)
