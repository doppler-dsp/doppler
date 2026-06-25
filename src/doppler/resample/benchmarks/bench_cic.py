"""Benchmark for CIC decimation filter.

Run: pytest src/doppler/resample/benchmarks/bench_cic.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.resample import CIC

BLOCK_64K = 65_536


@pytest.fixture
def cic_32():
    return CIC(32)


def test_bench_decimate_64k(benchmark, cic_32):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(cic_32.decimate, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )


@pytest.mark.parametrize("R", [4, 8, 32, 64, 256])
def test_bench_decimate_params(benchmark, R):
    cic = CIC(R)
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark.name = f"decimate_R{R}"
    benchmark(cic.decimate, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
