"""Benchmark for KalmanStateFilter.

Run, from the project root:  pytest --benchmark-only FILE, where FILE is

    src/doppler/track/benchmarks/bench_state_filter_kalman.py
"""

import numpy as np
import pytest

from doppler.track import KalmanStateFilter

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return KalmanStateFilter(bn=0.01)


def test_bench_step(benchmark, obj):
    benchmark(obj.step, 1.0)


def test_bench_steps_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.float64)
    benchmark(obj.steps, x)
