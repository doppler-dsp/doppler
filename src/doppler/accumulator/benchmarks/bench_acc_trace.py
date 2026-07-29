"""Benchmark for AccTrace.

Run: pytest src/doppler/accumulator/benchmarks/bench_acc_trace.py
     --benchmark-only
"""

import pytest

from doppler.accumulator import AccTrace

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return AccTrace(n=1024, mode="mean", alpha=0.1)
