"""Benchmark for Resampler and HalfbandDecimator.

Run: pytest src/doppler/resample/benchmarks/bench_resample.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.resample import HalfbandDecimator, Resampler, _halfband_bank

N = 1024


@pytest.fixture
def resamp_down():
    return Resampler(0.5)


@pytest.fixture
def resamp_up():
    return Resampler(2.0)


@pytest.fixture(scope="module")
def hb_fir():
    bank = _halfband_bank(60.0, 0.4, 0.6)
    centre = bank.shape[1] // 2
    row = 0 if abs(float(bank[0, centre])) < abs(float(bank[1, centre])) else 1
    return np.ascontiguousarray(bank[row])


@pytest.fixture
def hbdecim(hb_fir):
    return HalfbandDecimator(hb_fir)


def test_bench_resample_down(benchmark, resamp_down):
    x = np.ones(N, dtype=np.complex64)
    benchmark(resamp_down.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = N / benchmark.stats["mean"] / 1e6


def test_bench_resample_up(benchmark, resamp_up):
    x = np.ones(N, dtype=np.complex64)
    benchmark(resamp_up.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = N / benchmark.stats["mean"] / 1e6


def test_bench_hbdecim(benchmark, hbdecim):
    x = np.ones(N, dtype=np.complex64)
    benchmark(hbdecim.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = N / benchmark.stats["mean"] / 1e6
