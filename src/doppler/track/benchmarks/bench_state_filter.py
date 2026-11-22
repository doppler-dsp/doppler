# Hand-owned: the create row is not one jm generates.
"""Benchmark for StateFilter.

Run: pytest src/doppler/track/benchmarks/bench_state_filter.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.track import StateFilter

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return StateFilter()


def test_bench_step(benchmark, obj):
    benchmark(obj.step, 1.0)


def test_bench_create(benchmark):
    """Build and drop one filter: the gain solve, the binding, the free."""
    benchmark(StateFilter, 0.01, 1.0)


def test_bench_steps_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.float64)
    benchmark(obj.steps, x)
