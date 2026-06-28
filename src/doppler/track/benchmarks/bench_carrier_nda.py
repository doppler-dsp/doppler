"""Benchmark for CarrierNda.

Run: pytest src/doppler/track/benchmarks/bench_carrier_nda.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.track import CarrierNda

BLOCK_64K = 65_536
SPS = 8


@pytest.fixture
def obj():
    return CarrierNda(0.01, 0.707, 0.0, SPS, 4, 4)


@pytest.fixture
def rx():
    rng = np.random.default_rng(0)
    nsym = BLOCK_64K // SPS
    syms = np.exp(2j * np.pi * rng.integers(0, 4, nsym) / 4)
    sig = np.repeat(syms.astype(np.complex64), SPS)
    k = np.arange(len(sig))
    return (sig * np.exp(2j * np.pi * 0.002 * k)).astype(np.complex64)


def test_bench_steps_64k(benchmark, obj, rx):
    benchmark(obj.steps, rx)
    if benchmark.stats:
        obj.reset()
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
