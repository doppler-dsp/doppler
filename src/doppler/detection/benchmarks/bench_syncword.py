"""Benchmark for SyncFinder.

Run: pytest src/doppler/detection/benchmarks/bench_syncword.py --benchmark-only

`native/benchmarks/bench_syncword_core.c` measures the kernel and its scaling
with marker length. What only this face can show is what a Python receiver
actually pays: the per-call binding cost, and whether it matters against a
search window a synchroniser would really sweep.

The searches are for a marker that is NOT in the stream, which is the case a
receiver hunting for lock is in — every offset examined, no early exit.
"""

import numpy as np
import pytest

from doppler.detection import SyncFinder
from doppler.wfm import ccsds_asm_bits

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return SyncFinder(ccsds_asm_bits())


@pytest.fixture
def stream():
    """Random bits with no ASM in them.

    A 32-bit marker turns up in 64 Ki bits with probability about 3e-5, so
    this is reliably a full-window search rather than an early exit.
    """
    rng = np.random.default_rng(0)
    return rng.integers(0, 2, BLOCK_64K).astype(np.uint8)


def test_bench_find_64k(benchmark, obj, stream):
    benchmark(obj.find, stream, 4)


def test_bench_find_call_overhead(benchmark, obj):
    """The same call over 32 bits: one offset, so this is the binding alone."""
    x = np.zeros(32, dtype=np.uint8)
    benchmark(obj.find, x, 4)


def test_bench_max_errors_for(benchmark, obj):
    """Design-time, but a receiver may want it per acquisition."""
    benchmark(obj.max_errors_for, 4096, 1e-3)
