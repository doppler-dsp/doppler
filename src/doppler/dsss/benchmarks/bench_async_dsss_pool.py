"""Benchmark for AsyncDsssPool -- one epoch through three searching
receivers, from Python.

Run: pytest src/doppler/dsss/benchmarks/bench_async_dsss_pool.py
     --benchmark-only

The C twin (``native/benchmarks/bench_async_dsss_pool_core.c``) asks what
the workers buy; this row is what a Python caller pays per epoch for a pool
of three on one thread, the binding's call and the controller's step
included. The pool is fed noise, so every receiver searches all along, and
the row asserts that it did: a false alarm's refine would be timed as
search.
"""

import numpy as np

from doppler.dsss import AsyncDsssPool
from doppler.wfm import Gold

EPOCH = 2046
ROUNDS = 200


def test_push_epoch_search3(benchmark):
    code = np.asarray(Gold().generate(1023)).astype(np.uint8)
    pool = AsyncDsssPool(
        code,
        n_receivers=3,
        chip_rate=3.069e6,
        symbol_rate=2700.0,
        spc=2,
        cn0_dbhz=44.3,
        doppler_uncertainty=3000.0,
        threads=1,
    )
    rng = np.random.default_rng(7)
    n = EPOCH * (ROUNDS + 8)
    noise = (
        (rng.standard_normal(n) + 1j * rng.standard_normal(n)) * 0.7
    ).astype(np.complex64)
    blocks = iter(noise.reshape(-1, EPOCH))
    benchmark.pedantic(
        lambda: pool.push(next(blocks)), rounds=ROUNDS, warmup_rounds=1
    )
    for i in range(3):
        st = pool.status(i)
        assert st.state == 0 and st.state_samples == pool.samples
