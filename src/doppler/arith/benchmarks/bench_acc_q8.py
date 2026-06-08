"""Benchmark for AccQ8.

Run: pytest src/doppler/arith/benchmarks/bench_acc_q8.py --benchmark-only
"""

import pytest
import numpy as np

from doppler.arith import AccQ8

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return AccQ8(0)


def test_bench_step(benchmark, obj):
    benchmark(obj.step, 1)


def test_bench_steps_1k(benchmark, obj):
    x = np.ones(BLOCK_1K, dtype=np.int8)
    benchmark(obj.steps, x)


def test_bench_steps_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.int8)
    benchmark(obj.steps, x)
