"""Benchmark for Dll.

Run: pytest src/doppler/track/benchmarks/bench_dll.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.track import Dll

BLOCK_64K = 65_536
SF, SPS = 63, 4


@pytest.fixture
def code():
    return np.random.default_rng(0).integers(0, 2, SF).astype(np.uint8)


@pytest.fixture
def obj(code):
    return Dll(code, SPS, 0.0, 0.005, 0.707, 0.5)


@pytest.fixture
def rx(code):
    n = BLOCK_64K
    out = np.empty(n, np.complex64)
    cph = 0.0
    for k in range(n):
        idx = int(cph % SF)
        out[k] = -1.0 if code[idx] & 1 else 1.0
        cph += 1.0 / SPS
    return out


def test_bench_steps_64k(benchmark, obj, rx):
    benchmark(obj.steps, rx)
    if benchmark.stats:
        obj.reset()
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
