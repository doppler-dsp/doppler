"""Benchmark for DelayCf64.

Run: pytest src/doppler/delay/benchmarks/bench_delay.py --benchmark-only
"""

import pytest

from doppler.delay import DelayCf64

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return DelayCf64(BLOCK_1K)


def test_bench_push(benchmark, obj):
    benchmark(obj.push, 1.0 + 0.0j)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = 1 / benchmark.stats["mean"] / 1e6


def test_bench_push_ptr_1k(benchmark, obj):
    benchmark(obj.push_ptr, 1.0 + 0.0j)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_1K / benchmark.stats["mean"] / 1e6
        )


@pytest.fixture
def obj_64k():
    return DelayCf64(BLOCK_64K)


def test_bench_push_ptr_64k(benchmark, obj_64k):
    benchmark(obj_64k.push_ptr, 1.0 + 0.0j)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
