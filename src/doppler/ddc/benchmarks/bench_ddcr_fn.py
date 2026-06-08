"""Benchmark: functional DDCR API vs OO DDCR.

Run: pytest src/doppler/ddc/benchmarks/bench_ddcr_fn.py --benchmark-only

The functional API (ddcr_execute) takes a caller-supplied output buffer and
returns a zero-copy view.  The OO API (DDCR.execute) also returns a zero-copy
view into an internally pre-allocated buffer.  Both should show the same C
throughput; any delta is pure Python-call overhead.
"""

import pytest
import numpy as np

from doppler.ddc import (
    DDCR,
    ddcr_create,
    ddcr_execute,
    ddcr_destroy,
)

BLOCK_1K = 1_024
BLOCK_64K = 65_536


# ------------------------------------------------------------------ #
# Fixtures                                                             #
# ------------------------------------------------------------------ #


@pytest.fixture
def oo():
    return DDCR(0.0, 0.25)


@pytest.fixture
def fn():
    state = ddcr_create(0.0, 0.25)
    yield state
    ddcr_destroy(state)


# ------------------------------------------------------------------ #
# OO API                                                               #
# ------------------------------------------------------------------ #


def test_bench_oo_1k(benchmark, oo):
    x = np.ones(BLOCK_1K, dtype=np.float32)
    benchmark(oo.execute, x)


def test_bench_oo_64k(benchmark, oo):
    x = np.ones(BLOCK_64K, dtype=np.float32)
    benchmark(oo.execute, x)


# ------------------------------------------------------------------ #
# Functional API                                                       #
# ------------------------------------------------------------------ #


def test_bench_fn_1k(benchmark, fn):
    x = np.ones(BLOCK_1K, dtype=np.float32)
    out = np.empty(BLOCK_1K, dtype=np.complex64)
    benchmark(ddcr_execute, fn, x, out)


def test_bench_fn_64k(benchmark, fn):
    x = np.ones(BLOCK_64K, dtype=np.float32)
    out = np.empty(BLOCK_64K, dtype=np.complex64)
    benchmark(ddcr_execute, fn, x, out)
