"""Benchmark for DsssReceiver — searching, then tracking.

Run: pytest src/doppler/dsss/benchmarks/bench_dsss_receiver.py --benchmark-only

This file was a jm scaffold that took the `benchmark` fixture and never
called it, so the composed DSSS receiver reached the Python snapshot in no
row at all (doppler#1010).

Two rows, the same split as the C twin
(``native/benchmarks/bench_dsss_receiver_core.c``), because the receiver
does two different jobs at two very different per-sample costs and one
blended figure is the average of a transient and a steady state:

- ``search`` — 64k of noise, nothing to find. Every sample feeds the
  embedded `Acquisition` (the 2-D correlation), nothing is emitted. The
  price of listening.
- ``track``  — the receiver already locked, fed the NEXT contiguous 64k of
  the same capture each round: `Dll` -> `RateConverter` -> `MpskReceiver`
  per sample. What a continuous receiver pays for as long as it holds lock.

The waveform is NOT the C twin's: that one is a 7-chip toy at 35.7 ksym/s,
this is the 1023-chip Gold operating point the receiver's own tests validate
(`_async_stimulus.py`). Tracking cost scales with the symbol rate per
sample, so the faces agree on the split, not on the absolute track figure.

**Each row asserts it is still doing the job it is named for.** A search
that false-alarms switches to tracking mid-row; a tracker that has lost the
code still emits symbols at full rate. The search row asserts it never
locked; the track row asserts lock held AND every symbol it emitted decodes
bit-exactly against the transmitted bits, through `BerMeter`.
"""

import warnings

import numpy as np
import pytest

from doppler.dsss import DsssReceiver
from doppler.dsss.benchmarks._async_stimulus import (
    BLOCK_64K,
    CHIP_RATE,
    CODE,
    FS,
    SPC,
    SYM_RATE,
    TSYM,
    bit_errors,
    stream,
)
from doppler.wfm import Synth

ROUNDS = 30
#: Blocks fed untimed before the track row starts. Acquisition fires in the
#: first; the carrier lock EMA crosses 0.99 by about the eighth (measured:
#: 0.96 after five), and the row is named for the steady state.
PRIME = 10
#: Blocks past the timed rounds: pedantic's warmup round takes one, and
#: ``--benchmark-cprofile`` runs an extra profiled call that takes another.
#: An exhausted iterator raises StopIteration mid-benchmark, so leave room.
HEADROOM = 4


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
    """Listening: noise in, the acquisition searches, nothing is emitted."""
    noise = np.ascontiguousarray(
        np.asarray(Synth(type="noise", fs=FS, seed=3).steps(BLOCK_64K)).astype(
            np.complex64
        )
    )
    rx = _receiver()

    out = np.asarray(benchmark(rx.steps, noise))

    # Asserted, because a false alarm would flip the receiver to tracking
    # and the rest of the rounds would time the other regime under this
    # one's name.
    assert rx.tracking == 0, "noise acquired a signal; this is not search"
    assert out.size == 0, "the search row must emit nothing"
    _rate(benchmark)


def test_bench_steps_track(benchmark):
    """Locked: each round is the next contiguous 64k of the capture."""
    x, bits = stream(PRIME + ROUNDS + HEADROOM)
    blocks = iter(np.split(x, x.size // BLOCK_64K))
    rx = _receiver()
    syms = [rx.steps(next(blocks)) for _ in range(PRIME)]
    assert rx.tracking == 1, "never acquired; the row would time a search"

    # Built ONCE: state carries across calls by design, so each round must
    # be fed the capture's NEXT block, never the same one again — a replay
    # would jump the code phase and time a re-pull-in. The block is handed
    # over in pedantic's untimed setup; the append is the only work in the
    # timed call that is not the receiver's.
    benchmark.pedantic(
        lambda b: syms.append(rx.steps(b)),
        setup=lambda: ((next(blocks),), {}),
        rounds=ROUNDS,
        warmup_rounds=1,
    )

    assert rx.tracking == 1, "lost lock mid-row"
    assert rx.lock > 0.99, f"carrier lock {rx.lock:.3f}"
    # Counted from what ran, not from ROUNDS: a disabled benchmark (the
    # test suite's `--benchmark-disable`) makes one call, not ROUNDS + 1.
    timed = len(syms) - PRIME
    tracked = np.concatenate(syms[PRIME:])
    assert tracked.size == pytest.approx(timed * BLOCK_64K / TSYM, abs=2), (
        "a tracker that stops emitting reports a no-op as throughput"
    )
    every = np.concatenate(syms)
    errors, scored = bit_errors(every, bits, len(syms) * BLOCK_64K)
    assert errors == 0, (
        f"{errors} bit errors in {scored} symbols — this row would time a "
        "receiver that has stopped decoding"
    )
    # The marker the alignment was detected on is not scored; everything
    # else, the timed blocks included, is.
    assert scored >= every.size - 64, "the timed symbols were not all scored"
    if benchmark.stats:
        benchmark.extra_info["sym_s"] = (
            BLOCK_64K / TSYM / benchmark.stats["min"]
        )
    _rate(benchmark)
