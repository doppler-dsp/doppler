"""Benchmark for PN — m-sequence generation, both realizations.

Run: pytest src/doppler/wfm/benchmarks/bench_pn.py --benchmark-only

Six rows, ``generate[<lfsr>,len=<n>]``: 64k bits per call, Galois and
Fibonacci at register widths 11, 15 and 23 -- the grid the C benchmark
(``native/benchmarks/bench_pn_core.c``) times. Its finding is that all six
cost the same, because the limit is the serial per-bit feedback chain, not
the realization or the width; the Python rows say whether the binding
keeps that true.

Each register uses the default maximal-length polynomial (``poly=0``).
A reference instance checks that it IS one -- a full period carries
exactly ``2**(n-1)`` ones and the sequence repeats after ``2**n - 1`` --
and then the bits the TIMED call returned are located in that verified
sequence and must match it bit for bit. Any 64 consecutive bits of an
m-sequence with ``n < 64`` occur once per period, so the window is found
by search whatever number of rounds the timer chose to run. A generator
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

    # The property on the output that was TIMED, not on the reference: it
    # must be a contiguous window of the verified m-sequence.
    ring = np.concatenate([full[:period]] * (1 + BLOCK_64K // period + 1))
    pos = (
        ring.astype(np.uint8)
        .tobytes()
        .find(bits[:64].astype(np.uint8).tobytes())
    )
    assert 0 <= pos < period, "the timed bits are not in the m-sequence"
    assert np.array_equal(ring[pos : pos + BLOCK_64K], bits), (
        f"{lfsr},len={length}: the timed output leaves the m-sequence"
    )
    if benchmark.stats:
        sec = benchmark.stats["min"]
        benchmark.extra_info["MSa_s"] = BLOCK_64K / sec / 1e6
        benchmark.extra_info["Mbit_s"] = BLOCK_64K / sec / 1e6
