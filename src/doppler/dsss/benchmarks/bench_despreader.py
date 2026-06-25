"""Benchmark for Despreader.

Run: pytest src/doppler/dsss/benchmarks/bench_despreader.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.dsss import Despreader

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Despreader(np.zeros(1, dtype=np.uint8), 1, 2, 0.0, 0.0, 0.01, 0.002)
