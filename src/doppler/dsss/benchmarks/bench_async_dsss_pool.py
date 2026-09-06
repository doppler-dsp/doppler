"""Benchmark for AsyncDsssPool: one push() of one epoch through the
population -- twelve idle receivers and the searcher -- per input sample.

Run: pytest src/doppler/dsss/benchmarks/bench_async_dsss_pool.py
     --benchmark-only
"""

import numpy as np
import pytest

from doppler.dsss import AsyncDsssPool
from doppler.wfm import Gold

SF = 1023
SPC = 2
TE = SF * SPC


@pytest.fixture
def obj():
    code = np.asarray(Gold().generate(SF)).astype(np.uint8)
    return AsyncDsssPool(
        code,
        chip_rate=5e6,
        symbol_rate=2700.0,
        spc=SPC,
        cn0_dbhz=47.0,
        doppler_uncertainty=6000.0,
        n_slots=12,
    )


@pytest.fixture
def epoch():
    rng = np.random.default_rng(7)
    return (rng.standard_normal(TE) + 1j * rng.standard_normal(TE)).astype(
        np.complex64
    )


def test_push_epoch(benchmark, obj, epoch):
    benchmark(obj.push, epoch)
