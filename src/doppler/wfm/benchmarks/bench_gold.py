"""Benchmark for Gold — chip generation for a degree-10 preferred pair.

Run: pytest src/doppler/wfm/benchmarks/bench_gold.py --benchmark-only

One row, ``generate``: 64k chips per call from the CCSDS default pair, the
same configuration and block the C benchmark
(``native/benchmarks/bench_gold_core.c``) times. The gap between the faces
is the binding's per-call overhead.

The row asserts what a Gold code is, on the very chips it timed: periodic
in ``2**10 - 1`` and, from the three-valued correlation set ``{-1, -65,
63}`` the class documents, carrying 480, 512 or 544 ones per period. A
generator that degenerated -- a stuck register, a wrong tap mask -- fails
here rather than reading as a faster one.
"""

import numpy as np

from doppler.wfm import Gold

BLOCK_64K = 65_536
DEGREE = 10
PERIOD = 2**DEGREE - 1


def test_bench_generate(benchmark):
    g = Gold(taps_a=934, seed_a=350, taps_b=567, seed_b=73, length=DEGREE)
    # The phase a round starts at does not change its work, so no reset.
    chips = np.array(benchmark(g.generate, BLOCK_64K))  # a view: copy it
    assert chips.size == BLOCK_64K
    assert np.array_equal(chips[PERIOD:], chips[:-PERIOD])
    # sum of +-1 chips over a period is 1023 - 2w, one of {-1, -65, 63}
    assert int(chips[:PERIOD].sum()) in {480, 512, 544}
    if benchmark.stats:
        sec = benchmark.stats["min"]
        benchmark.extra_info["MSa_s"] = BLOCK_64K / sec / 1e6
        benchmark.extra_info["Mchip_s"] = BLOCK_64K / sec / 1e6
