"""Benchmark for InterpolatedTable.

Run: pytest src/doppler/interp/benchmarks/bench_interp_table.py
     --benchmark-only
"""

import numpy as np
import pytest

from doppler.interp import InterpolatedTable

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return InterpolatedTable(np.zeros(1, dtype=np.complex128), "linear")
