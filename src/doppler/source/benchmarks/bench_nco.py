"""Benchmark for NCO.

Run: pytest src/doppler/source/benchmarks/bench_nco.py --benchmark-only
"""

import pytest

from doppler.source import NCO

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return NCO(0.1, 0)


def test_bench_steps_u32_1k(benchmark, obj):
    benchmark(obj.steps_u32, BLOCK_1K)


def test_bench_steps_u32_64k(benchmark, obj):
    benchmark(obj.steps_u32, BLOCK_64K)


def test_bench_steps_u32_ovf_1k(benchmark, obj):
    benchmark(obj.steps_u32_ovf, BLOCK_1K)


def test_bench_steps_u32_ovf_64k(benchmark, obj):
    benchmark(obj.steps_u32_ovf, BLOCK_64K)
