# jm:generated bench_state_filter.py
# jm regenerates this file on apply while the line above is here.
# To make it yours, delete that line: jm then never writes it again.
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


def test_bench_steps_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.float64)
    benchmark(obj.steps, x)
