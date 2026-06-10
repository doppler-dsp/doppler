"""Benchmark for HBDecimQ15.

Run: pytest src/doppler/filter/benchmarks/bench_hbdecim_q15.py --benchmark-only
"""

import pytest
import numpy as np
from doppler.resample import _halfband_bank
from doppler.filter import HBDecimQ15

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture(scope="module")
def hb_fir():
    bank = _halfband_bank(60.0, 0.4, 0.6)
    c = bank.shape[1] // 2
    row = 0 if abs(float(bank[0, c])) < abs(float(bank[1, c])) else 1
    return np.ascontiguousarray(bank[row]).astype(np.float32)


@pytest.fixture
def dec(hb_fir):
    return HBDecimQ15(hb_fir)


def test_bench_execute_1k(benchmark, dec):
    x = np.zeros(2 * BLOCK_1K, dtype=np.int16)
    benchmark(dec.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_1K / benchmark.stats["mean"] / 1e6
        )


def test_bench_execute_64k(benchmark, dec):
    x = np.zeros(2 * BLOCK_64K, dtype=np.int16)
    benchmark(dec.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
