"""Benchmark for AWGN.

Run: pytest src/doppler/source/benchmarks/bench_awgn.py --benchmark-only
"""

import pytest

from doppler.source import AWGN

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return AWGN(0, 1.0)


def test_bench_generate_64k(benchmark, obj):
    benchmark(obj.generate, BLOCK_64K)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
