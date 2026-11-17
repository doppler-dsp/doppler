"""Benchmark for Wlan.

Run: pytest src/doppler/wlan/benchmarks/bench_wlan.py --benchmark-only
"""

import pytest

from doppler.wlan import Wlan


@pytest.fixture
def obj():
    return Wlan(constellation="qam16", code_rate="r34")


def test_bench_create(benchmark):
    benchmark(lambda: Wlan())


def test_bench_n_sym(benchmark, obj):
    benchmark(obj.n_sym, 1500)
