"""Benchmark for Acquirer.

Run: pytest src/doppler/dsss/benchmarks/bench_acq.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.dsss import Acquirer

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Acquirer(
        np.zeros(1, dtype=np.uint8), "mean", 1, 1, 16, 1e-3, 0.9, 0.1, 64
    )
