"""Benchmark for LO.

Run: pytest src/doppler/source/benchmarks/bench_lo.py --benchmark-only
"""

import pytest
import numpy as np

from doppler.source import LO

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return LO(0.1)


def test_bench_steps_1k(benchmark, obj):
    benchmark(obj.steps, BLOCK_1K)


def test_bench_steps_64k(benchmark, obj):
    benchmark(obj.steps, BLOCK_64K)


def test_bench_steps_ctrl_1k(benchmark, obj):
    ctrl = np.zeros(BLOCK_1K, dtype=np.float32)
    benchmark(obj.steps_ctrl, ctrl)


def test_bench_steps_ctrl_64k(benchmark, obj):
    ctrl = np.zeros(BLOCK_64K, dtype=np.float32)
    benchmark(obj.steps_ctrl, ctrl)
