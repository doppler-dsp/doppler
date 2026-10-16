"""Benchmark for DsssReceiver — searching.

Run: pytest src/doppler/dsss/benchmarks/bench_dsss_receiver.py --benchmark-only

This file was a jm scaffold that took the `benchmark` fixture and never
called it, so the composed DSSS receiver reached the Python snapshot in no
row at all (doppler#1010).

The receiver does two jobs at two very different per-sample costs, and one
blended figure is the average of a transient and a steady state, so each is
its own row:

- ``search`` (here) — 64k of noise, nothing to find, into a receiver reset
  before each of a fixed `ROUNDS`. Every sample feeds the embedded
  `Acquisition` (the 2-D correlation), nothing is emitted. The price of
  listening, on the 1023-chip Gold operating point the receiver's own tests
  validate (`_async_stimulus.py`). The C twin's search row timed zeros on a
  7-chip code and was cut, so this is the honest one.
- ``track`` — the C twin's
  (``native/benchmarks/bench_dsss_receiver_core.c``): `Dll` ->
  `RateConverter` -> `MpskReceiver` per sample, once locked. A Python copy
  here read the same cost per sample (35.3 against 34.7 MSa/s) and was cut
  (doppler#2386).

**The row asserts it is still doing the job it is named for.** A search
that false-alarms switches to tracking mid-row, so it asserts it never
locked and emitted nothing.
"""

import warnings

import numpy as np

from doppler.dsss import DsssReceiver
from doppler.dsss.benchmarks._async_stimulus import (
    BLOCK_64K,
    CHIP_RATE,
    CODE,
    FS,
    SPC,
    SYM_RATE,
)
from doppler.wfm import Synth

ROUNDS = 30


def _receiver():
    # The construction-time sizing advisory is not this file's subject.
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        return DsssReceiver(
            CODE,
            chip_rate=CHIP_RATE,
            symbol_rate=SYM_RATE,
            spc=SPC,
            cn0_dbhz=55.0,
            doppler_uncertainty=100.0,
            segments=4,
            sps=8,
        )


def _rate(benchmark):
    if benchmark.stats:
        # MIN, not mean: the least-disturbed observation, per
        # docs/dev/contributing/benchmarking.md.
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["min"] / 1e6
        )


def test_bench_steps_search(benchmark):
    """Listening: noise in, the acquisition searches, nothing is emitted.

    Reset before every round, through pedantic, not ``benchmark(rx.steps,
    noise)`` on one receiver: a receiver fed for as many calls as
    pytest-benchmark calibrates has a false-alarm exposure that grows with
    that count, so the assert below could pass on one machine and fail on
    another. Reset, every round is the same computation -- the same block
    into the same state -- so the verdict cannot depend on how many rounds
    run. The reset is in pedantic's untimed setup, and a reset receiver
    searches a block at the same cost as a warm one: the acquisition's
    work per block does not depend on what came before it.
    """
    noise = np.ascontiguousarray(
        np.asarray(Synth(type="noise", fs=FS, seed=3).steps(BLOCK_64K)).astype(
            np.complex64
        )
    )
    rx = _receiver()
    outs = []

    def fresh():
        rx.reset()
        return (noise,), {}

    benchmark.pedantic(
        lambda b: outs.append(np.asarray(rx.steps(b))),
        setup=fresh,
        rounds=ROUNDS,
        warmup_rounds=1,
    )

    # Asserted, because a false alarm would flip the receiver to tracking
    # and the rest of the rounds would time the other regime under this
    # one's name.
    assert rx.tracking == 0, "noise acquired a signal; this is not search"
    assert all(o.size == 0 for o in outs), "the search row must emit nothing"
    _rate(benchmark)
