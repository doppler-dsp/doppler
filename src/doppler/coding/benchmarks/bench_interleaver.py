"""Benchmark for Interleaver — and specifically for the UNIT.

The C benchmark (`bench_interleaver_core.c`) prices the kernel; this prices
what a Python caller pays, which includes the array marshaling. The arms are
the same pair, because the interesting number is the RATIO: an octet unit is
one memcpy per eight bits where a bit unit is one per bit, and that is a
parameter callers choose on correctness grounds without knowing it is also a
throughput one.

Run: pytest src/doppler/coding/benchmarks/bench_interleaver.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.coding import Interleaver

BLOCK_8K = 8_192
BLOCK_64K = 65_536


@pytest.mark.parametrize("n", [BLOCK_8K, BLOCK_64K])
@pytest.mark.parametrize("unit_bits", [1, 8])
def test_bench_interleave(benchmark, n, unit_bits):
    """Rate: bits permuted per second, at depth 8."""
    il = Interleaver(rows=8, cols=n // (8 * unit_bits), unit_bits=unit_bits)
    x = np.arange(n, dtype=np.uint8) & 1
    benchmark(il.interleave, x)


@pytest.mark.parametrize("n", [BLOCK_8K, BLOCK_64K])
def test_bench_deinterleave_soft(benchmark, n):
    """The receive path: LLRs, four bytes per unit rather than one."""
    il = Interleaver(rows=8, cols=n // 8, unit_bits=1)
    x = np.arange(n, dtype=np.float32)
    benchmark(il.deinterleave_soft, x)
