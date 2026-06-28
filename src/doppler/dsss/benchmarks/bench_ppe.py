"""Benchmark for PolyPhaseEstimator.

Run: pytest src/doppler/dsss/benchmarks/bench_ppe.py --benchmark-only
"""

import pytest

from doppler.dsss import PolyPhaseEstimator

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return PolyPhaseEstimator(4096, 0)
