"""Benchmark for RateConverter.

Run: pytest src/doppler/resample/benchmarks/bench_RateConverter.py
     --benchmark-only
"""

import numpy as np
import pytest

from doppler.resample import RateConverter

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def rc_hb():
    return RateConverter(0.5)  # HalfbandDecimator


@pytest.fixture
def rc_hb2():
    return RateConverter(0.25)  # HalfbandDecimator x2


@pytest.fixture
def rc_cic():
    return RateConverter(0.125)  # CIC(8)


@pytest.fixture
def rc_cic_resamp():
    return RateConverter(0.1)  # CIC(8) + Resampler(0.8)


@pytest.fixture
def rc_resamp():
    return RateConverter(1.0 / 3.0)  # Resampler only


@pytest.fixture
def rc_interp():
    return RateConverter(2.0)  # Resampler (interpolation)


def test_bench_hb_64k(benchmark, rc_hb):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(rc_hb.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )


def test_bench_hb2_64k(benchmark, rc_hb2):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(rc_hb2.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )


def test_bench_cic_64k(benchmark, rc_cic):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(rc_cic.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )


def test_bench_cic_resamp_64k(benchmark, rc_cic_resamp):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(rc_cic_resamp.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )


def test_bench_resamp_64k(benchmark, rc_resamp):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(rc_resamp.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )


def test_bench_interp_1k(benchmark, rc_interp):
    x = np.ones(BLOCK_1K, dtype=np.complex64)
    benchmark(rc_interp.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_1K / benchmark.stats["mean"] / 1e6
        )
