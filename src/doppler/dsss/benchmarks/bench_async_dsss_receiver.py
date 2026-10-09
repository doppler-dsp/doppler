"""Benchmark for AsyncDsssReceiver — a whole capture cold, and the held lock.

Run: pytest src/doppler/dsss/benchmarks/bench_async_dsss_receiver.py
     --benchmark-only

This file was a jm scaffold that took the `benchmark` fixture and never
called it, so the asynchronous DSSS receiver reached the Python snapshot in
no row at all (doppler#1010).

The receiver changes what it is doing partway through a capture — search,
then refine, then track — so the rows split the two regimes a caller sizes
against, the same split as the C twin
(``native/benchmarks/bench_async_dsss_receiver_core.c``):

- ``cold`` — a FRESH receiver over a whole 1M-sample capture in one call:
  acquisition, refinement, then tracking. What a burst-mode caller pays per
  capture, credited per sample of that capture.
- ``warm`` — one receiver, already tracking, fed the NEXT contiguous 64k of
  the same stream each round. What a continuous caller pays per sample for
  as long as it holds lock.

The waveform is the continuous Gold-code operating point in
`_async_stimulus.py`, not either of the C twin's two, so the faces agree on
the split rather than on absolute figures. The cell flavor
(`CellAsyncDsssReceiver`) is its own object and its own benchmark.

**Each row asserts it decoded what it was timed on.** Every stage exits
early on a signal that is not there, so a receiver that never acquires reads
FASTER, and one that has slipped off the code still emits symbols at full
rate. Both rows assert tracking at the end and zero bit errors against the
transmitted bits, through `BerMeter`.
"""

import numpy as np
import pytest

from doppler.dsss import AsyncDsssReceiver
from doppler.dsss.benchmarks._async_stimulus import (
    BLOCK_64K,
    CHIP_RATE,
    CODE,
    SPC,
    SYM_RATE,
    TSYM,
    bit_errors,
    stream,
)

#: The cold capture: 16 blocks, ~367 symbols — the same order as the C
#: twin's 400. Acquisition and refinement finish inside the first five.
COLD_BLOCKS = 16
COLD_ROUNDS = 10
WARM_ROUNDS = 30
#: Blocks fed untimed before the warm row starts: refinement hands over to
#: tracking in the fifth, and the lock EMA is past 0.99 by the tenth.
PRIME = 12


def _receiver():
    return AsyncDsssReceiver(
        CODE, chip_rate=CHIP_RATE, symbol_rate=SYM_RATE, spc=SPC
    )


def _rate(benchmark, n):
    if benchmark.stats:
        # MIN, not mean: the least-disturbed observation, per
        # docs/dev/contributing/benchmarking.md.
        benchmark.extra_info["MSa_s"] = n / benchmark.stats["min"] / 1e6


def _decodes(syms, bits, n_fed, what):
    errors, scored = bit_errors(syms, bits, n_fed)
    assert errors == 0, (
        f"{what}: {errors} bit errors in {scored} symbols — this row would "
        "time a receiver that has stopped decoding"
    )
    # The marker the alignment was detected on is not scored; the rest is.
    assert scored >= syms.size - 64, f"{what}: not every symbol was scored"


def test_bench_steps_cold(benchmark):
    """A fresh receiver over one whole capture: search, refine, track."""
    x, bits = stream(COLD_BLOCKS)
    holder = []

    # Fresh per round, built untimed in pedantic's setup: a reused receiver
    # is already tracking from round two, and would time the warm row.
    def run(rx):
        holder[:] = [rx]
        return rx.steps(x)

    out = np.asarray(
        benchmark.pedantic(
            run,
            setup=lambda: ((_receiver(),), {}),
            rounds=COLD_ROUNDS,
            warmup_rounds=1,
        )
    )

    rx = holder[0]
    assert rx.tracking == 1, "the capture ended without a lock"
    assert out.size > COLD_BLOCKS * BLOCK_64K / TSYM / 2, (
        f"only {out.size} symbols: the row is mostly search, not a capture"
    )
    _decodes(out, bits, x.size, "cold")
    _rate(benchmark, x.size)


def test_bench_steps_warm(benchmark):
    """Held lock: each round is the next contiguous 64k of the stream."""
    x, bits = stream(PRIME + WARM_ROUNDS + 1)  # +1: the warmup round
    blocks = iter(np.split(x, x.size // BLOCK_64K))
    rx = _receiver()
    syms = [rx.steps(next(blocks)) for _ in range(PRIME)]
    assert rx.tracking == 1, "never acquired; the row would time a search"

    # Built ONCE and fed the NEXT block every round — a replayed block would
    # jump the code phase and time a re-pull-in. The block is handed over in
    # pedantic's untimed setup.
    benchmark.pedantic(
        lambda b: syms.append(rx.steps(b)),
        setup=lambda: ((next(blocks),), {}),
        rounds=WARM_ROUNDS,
        warmup_rounds=1,
    )

    assert rx.tracking == 1, "lost lock mid-row"
    assert rx.lock > 0.99, f"carrier lock {rx.lock:.3f}"
    # Counted from what ran: `--benchmark-disable` makes one call, not
    # WARM_ROUNDS + 1.
    timed = len(syms) - PRIME
    tracked = np.concatenate(syms[PRIME:])
    assert tracked.size == pytest.approx(timed * BLOCK_64K / TSYM, abs=2), (
        "a tracker that stops emitting reports a no-op as throughput"
    )
    _decodes(np.concatenate(syms), bits, len(syms) * BLOCK_64K, "warm")
    if benchmark.stats:
        benchmark.extra_info["sym_s"] = (
            BLOCK_64K / TSYM / benchmark.stats["min"]
        )
    _rate(benchmark, BLOCK_64K)
