"""Benchmark for PN — m-sequence generation, both realizations.

Run: pytest src/doppler/wfm/benchmarks/bench_pn.py --benchmark-only

Six rows, ``generate[<lfsr>,len=<n>]``: 64k bits per call, Galois and
Fibonacci at register widths 11, 15 and 23 -- the grid the C benchmark
(``native/benchmarks/bench_pn_core.c``) times. Its finding is that all six
cost the same, because the limit is the serial per-bit feedback chain, not
the realization or the width; the Python rows say whether the binding
keeps that true.

Each register uses the default maximal-length polynomial (``poly=0``), and
before timing the row checks that it IS one: a full period carries exactly
``2**(n-1)`` ones and the sequence repeats after ``2**n - 1``. A generator
that fell off its maximal length would otherwise just produce bits at the
same speed.
"""

import numpy as np
import pytest

from doppler.wfm import PN

BLOCK_64K = 65_536
GRID = [(lfsr, n) for lfsr in ("galois", "fibonacci") for n in (11, 15, 23)]


@pytest.mark.parametrize(
    ("lfsr", "length"), GRID, ids=[f"{k},len={n}" for k, n in GRID]
)
def test_bench_generate(benchmark, lfsr, length):
    period = 2**length - 1
    full = np.array(PN(seed=1, length=length, lfsr=lfsr).generate(period + 64))
    assert int(full[:period].sum()) == 2 ** (length - 1)
    assert np.array_equal(full[period:], full[:64])

    p = PN(seed=1, length=length, lfsr=lfsr)
    bits = np.array(benchmark(p.generate, BLOCK_64K))  # a view: copy it
    assert bits.size == BLOCK_64K
    if benchmark.stats:
        sec = benchmark.stats["min"]
        benchmark.extra_info["MSa_s"] = BLOCK_64K / sec / 1e6
        benchmark.extra_info["Mbit_s"] = BLOCK_64K / sec / 1e6
