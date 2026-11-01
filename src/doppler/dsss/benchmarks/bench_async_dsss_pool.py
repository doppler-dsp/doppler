"""Benchmark for AsyncDsssPool -- one dwell through a pool of four idle
receivers and the controller's one search, from Python.

Run: pytest src/doppler/dsss/benchmarks/bench_async_dsss_pool.py
     --benchmark-only

The pinned operating point, the same as the C twin
(``native/benchmarks/async_dsss_pool/bench_async_dsss_pool_core.c``): a
1,023-chip Gold code at 5 Mcps captured at 11 MSa/s, through one
``RateConverter`` to 10 MSa/s
inside the timed call, 3 ksym/s, the search block-coherent over the depth
a 500-symbol data-free block and a 500 Hz/s Doppler rate allow, over one
slow-time tile.
The C rows ask how the cost scales with receivers, span and threads; this
row is what a Python caller pays per dwell, the bindings' calls and the
controller's steps included. The pool is fed noise, so every receiver
stays idle and only the controller searches, and the row asserts it: every
receiver idle for every whole code period fed (a false alarm's seed would be
timed as a refine).
"""

import numpy as np

from doppler.dsss import AsyncDsssPool
from doppler.resample import RateConverter
from doppler.wfm import Gold

CHIP, FS_IN, SPC = 5e6, 11e6, 2
ROUNDS = 6


def test_push_dwell_native4(benchmark):
    code = np.asarray(Gold().generate(1023)).astype(np.uint8)
    sym = 3000.0  # the link's symbol rate
    pool = AsyncDsssPool(
        code,
        n_receivers=4,
        chip_rate=CHIP,
        symbol_rate=sym,
        sample_rate=CHIP * SPC,
        cn0_dbhz=10.0 + 10.0 * np.log10(sym),
        doppler_uncertainty=CHIP / 1023 / 2,  # one tile: +/- 1 / (2 T_E)
        code_only_s=500 / sym,  # 500 data-free symbols
        doppler_rate=500.0,
        threads=1,
    )
    rc = RateConverter(rate=CHIP * SPC / FS_IN, compensate=0)
    dwell = pool.coherent_bins * 1023 * SPC  # receiver samples
    block = round(dwell * FS_IN / (CHIP * SPC))  # input samples
    rng = np.random.default_rng(7)
    n = block * (ROUNDS + 2)
    noise = (
        (rng.standard_normal(n) + 1j * rng.standard_normal(n)) * 0.7
    ).astype(np.complex64)
    blocks = iter(noise.reshape(-1, block))
    benchmark.pedantic(
        lambda: pool.push(rc.execute(next(blocks))),
        rounds=ROUNDS,
        warmup_rounds=1,
    )
    period = 1023 * SPC  # status is as of the last whole code period
    fed = pool.samples - pool.samples % period
    for i in range(4):
        st = pool.status(i)
        assert st.state == 3 and st.state_samples == fed  # idle throughout
