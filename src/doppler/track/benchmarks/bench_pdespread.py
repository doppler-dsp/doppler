"""Benchmark for PartialDespreader.

Run: pytest src/doppler/track/benchmarks/bench_pdespread.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.track import PartialDespreader

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return PartialDespreader(
        np.zeros(1, dtype=np.uint8), 4, 4, 0.0, 0.002, 0.707, 0.5
    )
