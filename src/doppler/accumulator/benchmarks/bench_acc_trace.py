"""Benchmark for AccTrace.

Run: pytest src/doppler/accumulator/benchmarks/bench_acc_trace.py --benchmark-only
"""

import pytest

from doppler.accumulator import AccTrace

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return AccTrace("mean", 1024, 0.1)
