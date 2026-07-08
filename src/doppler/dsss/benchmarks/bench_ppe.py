"""Benchmark for PolynomialPhaseEstimator.

Run: pytest src/doppler/dsss/benchmarks/bench_ppe.py --benchmark-only
"""

import pytest

from doppler.dsss import PolynomialPhaseEstimator

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return PolynomialPhaseEstimator(4096, 0)
