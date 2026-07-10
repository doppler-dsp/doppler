"""Benchmark for LockDet.

Run: pytest src/doppler/detection/benchmarks/bench_lockdet.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.detection import LockDet

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return LockDet(1.0, 1.0, 1, 1)


def test_bench_step(benchmark, obj):
    benchmark(obj.step, 1.0)


def test_bench_steps_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.float64)
    benchmark(obj.steps, x)
