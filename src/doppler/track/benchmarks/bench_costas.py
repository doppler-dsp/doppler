"""Benchmark for Costas.

Run: pytest src/doppler/track/benchmarks/bench_costas.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.track import Costas

BLOCK_64K = 65_536
TSAMPS = 16


@pytest.fixture
def obj():
    return Costas(0.05, 0.707, 0.0, TSAMPS)


@pytest.fixture
def rx():
    rng = np.random.default_rng(0)
    nsym = BLOCK_64K // TSAMPS
    bits = rng.integers(0, 2, nsym) * 2 - 1
    sig = np.repeat(bits.astype(np.complex64), TSAMPS)
    k = np.arange(len(sig))
    return (sig * np.exp(2j * np.pi * 0.002 * k)).astype(np.complex64)


def test_bench_steps_64k(benchmark, obj, rx):
    benchmark(obj.steps, rx)
    if benchmark.stats:
        obj.reset()
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
