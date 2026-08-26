"""Benchmark for BurstAcquisition — the burst chain's search stage.

Run: pytest src/doppler/dsss/benchmarks/bench_burst_acq.py --benchmark-only

The same two rows, over the same 64k stimulus, as every other object in the
chain (`_burst_stimulus.py`) — so this number reads against
`DsssBurstReceiver`'s rather than in isolation. This is the stage that runs
on EVERY sample, so `push_idle` here is very nearly the composed object's
`push_idle`: the difference is the composed object's own book-keeping, not
another correlation.

- ``push_idle``   — 64k of noise. What listening costs.
- ``push_bursts`` — the same 64k carrying four bursts.

**The idle row is not asserted to be empty, and that is the honest
reading.** At `pfa = 1e-3` over a surface this size a false alarm is
expected, not a defect — the gate is priced to allow them. What separates
the rows is that the burst block yields many more hits, and raw
acquisition emits SEVERAL per burst: coalescing them into one detection per
preamble is `DsssBurstReceiver`'s job (its `refine_span` window), not this
object's. Asserting `idle == 0` would be asserting the false-alarm rate is
zero, which would make the row fail for behaving correctly.
"""

import pytest

from doppler.dsss import BurstAcquisition
from doppler.dsss.benchmarks._burst_stimulus import (
    CHIP_RATE,
    REPS,
    SPC,
    burst_stimulus,
    packing,
    rate,
)


@pytest.fixture(scope="module")
def waveform():
    return burst_stimulus()


@pytest.fixture(scope="module")
def n_bursts(waveform):
    """How many bursts the stimulus actually put in the block.

    Read from the receiver's own `refine_span` rather than assumed: pack
    them tighter and they coalesce, so a hardcoded count would quietly
    assert against a block that holds fewer.
    """
    acq_code, data_code, *_ = waveform
    return packing(acq_code, data_code)[1]


def _acq(acq_code):
    return BurstAcquisition(
        acq_code, REPS, SPC, CHIP_RATE, 60.0, 0.0, 1e-3, 0.9, "mean"
    )


def test_bench_push_idle(benchmark, waveform):
    """The search floor — the cost paid on every sample, burst or not."""
    acq_code, _, _, _, idle = waveform
    # Rebuilt per round: the engine carries a dwell accumulator and a ring,
    # so a reused instance would measure a warm one from round two onward.
    hits = benchmark(lambda: _acq(acq_code).push(idle))
    assert isinstance(hits, list)
    rate(benchmark)


def test_bench_push_bursts(benchmark, waveform, n_bursts):
    """Four bursts in the block — what a detection adds to the search."""
    acq_code, _, _, bursts, idle = waveform
    hits = benchmark(lambda: _acq(acq_code).push(bursts))
    # Not "== N_BURSTS": raw acquisition emits several hits per preamble and
    # the chain coalesces them downstream. What must hold is that the block
    # with bursts in it detects substantially more than the one without.
    idle_hits = len(_acq(acq_code).push(idle))
    assert len(hits) >= n_bursts, (
        f"only {len(hits)} hits for {n_bursts} bursts — this row is timing "
        "a search that is no longer detecting"
    )
    assert len(hits) > idle_hits, (
        f"{len(hits)} hits on bursts vs {idle_hits} on noise — the two rows "
        "are not measuring different work"
    )
    rate(benchmark)
