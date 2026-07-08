"""Benchmark for BurstDespreader.

Run: pytest src/doppler/dsss/benchmarks/bench_burst_despreader.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.dsss import BurstDespreader

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return BurstDespreader(
        np.zeros(1, dtype=np.uint8), 1, 2, 0.0, 0.0, 0.01, 0.002
    )
