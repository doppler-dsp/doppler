"""Benchmark for Despreader.

Run: pytest src/doppler/dsss/benchmarks/bench_despreader.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.dsss import Despreader

BLOCK_64K = 65_536
SF, SPS = 127, 8


@pytest.fixture
def code():
    return np.random.default_rng(0).integers(0, 2, SF).astype(np.uint8)


@pytest.fixture
def obj(code):
    return Despreader(code, SPS, 0.0, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5, 1)


@pytest.fixture
def rx(code):
    n = BLOCK_64K
    out = np.empty(n, np.complex64)
    cph = 0.0
    for k in range(n):
        idx = int(cph % SF)
        out[k] = -1.0 if code[idx] & 1 else 1.0
        cph += 1.0 / SPS
    return (out * np.exp(2j * np.pi * 5e-5 * np.arange(n))).astype(
        np.complex64
    )


def test_bench_steps_64k(benchmark, obj, rx):
    benchmark(obj.steps, rx)
    if benchmark.stats:
        obj.reset()
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
