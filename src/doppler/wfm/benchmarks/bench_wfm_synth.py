"""Benchmark for Synth.

Run: pytest src/doppler/wfm/benchmarks/bench_synth.py --benchmark-only
"""

import pytest

from doppler.wfm import Synth

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Synth(type="tone", fs=1000000.0, freq=0.0, snr=100.0, seed=1)


def test_bench_step(benchmark, obj):
    benchmark(obj.step)


def test_bench_steps_1k(benchmark, obj):
    benchmark(obj.steps, BLOCK_1K)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_1K / benchmark.stats["mean"] / 1e6
        )


def test_bench_steps_64k(benchmark, obj):
    benchmark(obj.steps, BLOCK_64K)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
