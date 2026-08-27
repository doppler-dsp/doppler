"""Benchmark for BurstDemod — the burst chain's feedforward last stage.

Run: pytest src/doppler/dsss/benchmarks/bench_burst_demod.py --benchmark-only

The same two rows over the same 64k stimulus as the rest of the chain
(`_burst_stimulus.py`). Unlike the search stage, this one's cost DOES
depend on what it is handed: `demod()` estimates frequency and chirp rate
over the preamble, dechirps, despreads, hunts the sync word and slices, and
it does all of that whether or not a frame is really there.

- ``demod_idle``  — 64k of noise. The frame it hands back fails its own
  trailer, which is the caller's check, not this object's.
- ``demod_burst`` — 64k whose first burst decodes, bits exact.

**Both rows do the same work**, which is the point worth knowing: a
feedforward demodulator cannot decide early that there is nothing to
demodulate — it has no loop whose failure to converge would tell it, and it
does not check the frame either (doppler#1022). Nothing about the noise row
is a cheap path, so a caller sizing for the worst case should read it
rather than the burst one.
"""

import numpy as np
import pytest

from doppler.dsss import BurstDemod
from doppler.dsss.benchmarks._burst_stimulus import (
    CHIP_RATE,
    FRAME_SYMS,
    PAYLOAD,
    PAYLOAD_OFF,
    REPS,
    SPC,
    SYNC,
    burst_stimulus,
    rate,
)


@pytest.fixture(scope="module")
def waveform():
    return burst_stimulus()


def _demod(acq_code, data_code):
    d = BurstDemod(data_code, SPC, CHIP_RATE, 0.0, 0.0, FRAME_SYMS, 10)
    d.set_preamble(acq_code, REPS)
    d.set_sync(SYNC)
    return d


def test_bench_demod_idle(benchmark, waveform):
    """Noise in: the full estimate/dechirp/despread/slice path still runs."""
    acq_code, data_code, _, _, idle = waveform
    d = _demod(acq_code, data_code)
    out = np.asarray(benchmark(lambda: d.demod(idle)))
    # The negative case, checked where a caller checks it: noise cannot
    # produce a frame whose own trailer matches its own payload.
    from doppler.wfm import crc16

    rx = 0
    for b in out[PAYLOAD_OFF + PAYLOAD :][:16]:
        rx = (rx << 1) | (int(b) & 1)
    assert rx != int(crc16(out[PAYLOAD_OFF : PAYLOAD_OFF + PAYLOAD])), (
        "noise produced a frame that checks out — this row is no longer the "
        "negative case it claims to be"
    )
    rate(benchmark)


def test_bench_demod_burst(benchmark, waveform):
    """A real burst in the block: same path, and it has to come out right."""
    acq_code, data_code, payload, bursts, _ = waveform
    d = _demod(acq_code, data_code)
    out = np.asarray(benchmark(lambda: d.demod(bursts)))
    assert out.size == FRAME_SYMS, "the FRAME comes back, not the payload"
    assert np.array_equal(out[PAYLOAD_OFF : PAYLOAD_OFF + PAYLOAD], payload), (
        "payload did not come back bit-exact — this row would otherwise time "
        "a demodulator that had stopped working"
    )
    rate(benchmark)
