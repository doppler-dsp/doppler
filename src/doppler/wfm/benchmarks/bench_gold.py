"""Benchmark for Gold.

Run: pytest src/doppler/wfm/benchmarks/bench_gold.py --benchmark-only
"""

import pytest

from doppler.wfm import Gold

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Gold(934, 350, 567, 73, 10)
