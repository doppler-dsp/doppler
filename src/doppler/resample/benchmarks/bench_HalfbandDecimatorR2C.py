# jm:generated bench_HalfbandDecimatorR2C.py
# jm regenerates this file on apply while the line above is here.
# To make it yours, delete that line: jm then never writes it again.
"""Benchmark for HalfbandDecimatorR2C.

Run, from the project root:  pytest --benchmark-only FILE, where FILE is

    src/doppler/resample/benchmarks/bench_HalfbandDecimatorR2C.py
"""

import numpy as np
import pytest

from doppler.resample import HalfbandDecimatorR2C

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return HalfbandDecimatorR2C(h=np.zeros(1, dtype=np.float32))


def test_bench_create(benchmark):
    benchmark(lambda: HalfbandDecimatorR2C(h=np.zeros(1, dtype=np.float32)))
