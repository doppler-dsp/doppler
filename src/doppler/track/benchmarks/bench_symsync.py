"""Benchmark for SymbolSync.

Run: pytest src/doppler/track/benchmarks/bench_symsync.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.track import SymbolSync

BLOCK_64K = 65_536
SPS = 4


@pytest.fixture
def obj():
    return SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic")


@pytest.fixture
def rx():
    n = BLOCK_64K
    sym = np.repeat(
        np.random.default_rng(0).integers(0, 2, n // SPS) * 2 - 1, SPS
    )
    return sym[:n].astype(np.complex64)


def test_bench_steps_64k(benchmark, obj, rx):
    benchmark(obj.steps, rx)
    if benchmark.stats:
        obj.reset()
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
