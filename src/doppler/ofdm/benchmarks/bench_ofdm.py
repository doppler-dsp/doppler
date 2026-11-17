# jm:generated bench_ofdm.py
# jm regenerates this file on apply while the line above is here.
# To make it yours, delete that line: jm then never writes it again.
"""Benchmark for Ofdm.

Run: pytest src/doppler/ofdm/benchmarks/bench_ofdm.py --benchmark-only
"""

import pytest

from doppler.ofdm import Ofdm

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Ofdm()


def test_bench_create(benchmark):
    benchmark(lambda: Ofdm())
