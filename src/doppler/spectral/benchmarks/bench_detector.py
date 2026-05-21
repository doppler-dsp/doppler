"""Benchmark for Detector.

Run: pytest src/doppler/spectral/benchmarks/bench_detector.py --benchmark-only
"""
import pytest
import numpy as np

from doppler.spectral import Detector

BLOCK_1K  = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Detector(np.zeros(1, dtype=np.complex64), "mean", 1, 0, n-1, 0.0, 1)

def test_bench_step(benchmark, obj):
    benchmark(obj.step, 1.0 + 0.0j)


def test_bench_steps_1k(benchmark, obj):
    x = np.ones(BLOCK_1K, dtype=np.complex64)
    benchmark(obj.steps, x)

def test_bench_steps_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(obj.steps, x)
