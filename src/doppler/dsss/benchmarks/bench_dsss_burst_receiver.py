"""Benchmark for DsssBurstReceiver — the composed burst chain.

Run: pytest src/doppler/dsss/benchmarks/bench_dsss_burst_receiver.py
     --benchmark-only

Two rows over the same 64k block, because `push()`'s three stages do not
run equally often. **Search** runs on every sample and sets the sustained
rate a caller can feed. **Refine** and **demod** run once per detection --
a per-burst cost, amortised over whatever separates one burst from the
next. One blended figure hides which of the two a caller pays for, and the
duty cycle that would blend them correctly is the caller's, not this
file's.

- ``push_idle``   — 64k of noise, nothing decodes. The price of listening.
- ``push_bursts`` — the same 64k carrying four bursts, all decoded. Four
  rather than one so the per-detection cost is a slope across the block
  instead of a single event lost in it, and so the dedup path (one
  detection's suppression window against the next burst) is exercised.

The chain's parts carry the SAME two rows over the SAME stimulus
(`_burst_stimulus.py`), so this number can be read against them rather than
in isolation. The C benchmark
(``native/benchmarks/bench_dsss_burst_receiver_core.c``) measures the same
split; the gap between the two faces is the binding's per-call overhead,
which is what a Python benchmark is for.
"""

import numpy as np
import pytest

from doppler.dsss import DsssBurstReceiver
from doppler.dsss.benchmarks._burst_stimulus import (
    CHIP_RATE,
    FRAME_SYMS,
    PAYLOAD,
    PAYLOAD_OFF,
    REPS,
    SPC,
    SYNC,
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


def _receiver(acq_code, data_code):
    return DsssBurstReceiver(
        acq_code=acq_code,
        data_code=data_code,
        sync=SYNC,
        reps=REPS,
        spc=SPC,
        chip_rate=CHIP_RATE,
        frame_syms=FRAME_SYMS,
        cn0_dbhz=60.0,
        doppler_uncertainty=0.0,
        pfa=1e-3,
        pd=0.9,
        carrier_hz=0.0,
        max_rate=0.0,
        est_segments=10,
    )


def test_bench_push_idle(benchmark, waveform):
    """The search floor: 64k of noise in, nothing decodes."""
    acq_code, data_code, _, _, idle = waveform

    def run():
        # Rebuilt per round: push() carries look-back history and a
        # suppression window, so reusing one instance would measure the
        # dedup path from the second round onward rather than the search.
        return _receiver(acq_code, data_code).push(idle)

    out = benchmark(run)
    assert len(out) == 0, "the idle row must decode nothing, or it is not idle"
    rate(benchmark)


def test_bench_push_bursts(benchmark, waveform, n_bursts):
    """Search plus four decoded bursts — the per-detection cost, on top."""
    acq_code, data_code, payload, bursts, _ = waveform

    def run():
        return _receiver(acq_code, data_code).push(bursts)

    out = np.asarray(benchmark(run))
    # Asserted, because a benchmark that quietly stopped decoding would
    # otherwise simply look faster.
    # The receiver hands back FRAMES — it stops at decisions, so the payload
    # is a slice a caller takes (doppler#1022).
    assert out.size == n_bursts * FRAME_SYMS, (
        f"expected {n_bursts} bursts, decoded {out.size // FRAME_SYMS} — this "
        "row would otherwise time the search path under the decode path's "
        "name"
    )
    for k in range(n_bursts):
        frame = out[k * FRAME_SYMS : (k + 1) * FRAME_SYMS]
        assert np.array_equal(
            frame[PAYLOAD_OFF : PAYLOAD_OFF + PAYLOAD], payload
        ), f"burst {k} did not decode bit-exactly"
    rate(benchmark)
