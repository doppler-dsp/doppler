"""Benchmark for SymbolTimingEstimator: one estimate over a 64k block.

Run: pytest src/doppler/estimation/benchmarks/bench_timing_est.py
     --benchmark-only

One row. The object does one thing a sample (a power, and a rotation of it
onto the symbol clock), so its cost is per input sample and there is no
second configuration that changes the work: `sps` only sets the clock's
step. The C twin (``native/benchmarks/timing_est/``) times the same block
length, so the gap between the faces is the binding's per-call overhead.

The row asserts the estimate it timed, because an estimator that returned
early would look faster: the stream's power peaks 0.2 symbols into each
symbol and the timed call has to say so.
"""

import numpy as np
import pytest

from doppler.estimation import SymbolTimingEstimator

BLOCK_64K = 65_536
SPS = 4.0
TAU = 0.2


@pytest.fixture
def block():
    i = np.arange(BLOCK_64K)
    p = 1.0 + 0.5 * np.cos(2 * np.pi * (i / SPS - TAU))
    return (np.sqrt(p) * np.exp(1j * 0.2 * i)).astype(np.complex64)


def test_bench_estimate_64k(benchmark, block):
    te = SymbolTimingEstimator(sps=SPS)
    e = benchmark(te.estimate, block)
    assert abs(e.tau - TAU) < 1e-3
    assert e.symbols == BLOCK_64K / SPS
    # No stats when the suite runs with benchmarks disabled (one untimed
    # call): the estimate above is still asserted, only the rate is skipped.
    if benchmark.stats:
        benchmark.extra_info["MSa/s"] = round(
            BLOCK_64K / benchmark.stats["mean"] / 1e6, 1
        )
