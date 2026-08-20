"""Benchmark for Viterbi.

Run: pytest src/doppler/viterbi/benchmarks/bench_viterbi.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.viterbi import Viterbi

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Viterbi(poly=np.zeros(1, dtype=np.uint32), k=7, invert=0, depth=35)
