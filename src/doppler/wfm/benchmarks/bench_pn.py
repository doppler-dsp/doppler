"""Benchmark for PN.

Run: pytest src/doppler/wfm/benchmarks/bench_pn.py --benchmark-only
"""

import pytest

from doppler.wfm import PN

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return PN(96, 1, 7)
