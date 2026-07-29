"""Benchmark for ToneMeasure.

Run: pytest src/doppler/measure/benchmarks/bench_tonemeas.py --benchmark-only
"""

import pytest

from doppler.measure import ToneMeasure

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return ToneMeasure(n=8192, fs=1.0, n_harmonics=8, full_scale=1.0)
