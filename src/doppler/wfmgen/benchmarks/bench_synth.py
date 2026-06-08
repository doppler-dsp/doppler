"""Benchmark for Synth.

Run: pytest src/doppler/wfmgen/benchmarks/bench_synth.py --benchmark-only
"""

import pytest

from doppler.wfmgen import Synth

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Synth(type="tone", fs=1000000.0, freq=0.0, snr=100.0, seed=1)


def test_bench_step(benchmark, obj):
    benchmark(obj.step)


def test_bench_steps_1k(benchmark, obj):
    benchmark(obj.steps, BLOCK_1K)


def test_bench_steps_64k(benchmark, obj):
    benchmark(obj.steps, BLOCK_64K)
