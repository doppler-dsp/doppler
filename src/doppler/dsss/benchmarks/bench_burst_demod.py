"""Benchmark for BurstDemod — the burst chain's feedforward last stage.

Run: pytest src/doppler/dsss/benchmarks/bench_burst_demod.py --benchmark-only

The same two rows over the same 64k stimulus as the rest of the chain
(`_burst_stimulus.py`). Unlike the search stage, this one's cost DOES
depend on what it is handed: `demod()` estimates frequency and chirp rate
over the preamble, dechirps, despreads, hunts the sync word and slices, and
it does all of that whether or not a frame is really there.

- ``demod_idle``  — 64k of noise. `frame_valid` comes back 0.
- ``demod_burst`` — 64k whose first burst decodes, CRC valid, bits exact.

**Both rows do the same work**, which is the point worth knowing: a
feedforward demodulator cannot decide early that there is nothing to
demodulate — it has no loop whose failure to converge would tell it. The
CRC at the end is the first moment it knows, so the noise row is not a
cheap path and a caller sizing for the worst case should read the idle row,
not the burst one.
"""

import numpy as np
import pytest

from doppler.dsss import BurstDemod
from doppler.dsss.benchmarks._burst_stimulus import (
    CHIP_RATE,
    PAYLOAD,
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
    d = BurstDemod(data_code, SPC, CHIP_RATE, 0.0, 0.0, PAYLOAD, 10)
    d.set_preamble(acq_code, REPS)
    d.set_frame(SYNC)
    return d


def test_bench_demod_idle(benchmark, waveform):
    """Noise in: the full estimate/dechirp/despread/slice path still runs."""
    acq_code, data_code, _, _, idle = waveform
    d = _demod(acq_code, data_code)
    benchmark(lambda: d.demod(idle))
    assert d.frame_valid == 0, (
        "noise reported a valid frame — the CRC check is not doing its job, "
        "and this row is no longer the negative case it claims to be"
    )
    rate(benchmark)


def test_bench_demod_burst(benchmark, waveform):
    """A real burst in the block: same path, and it has to come out right."""
    acq_code, data_code, payload, bursts, _ = waveform
    d = _demod(acq_code, data_code)
    out = np.asarray(benchmark(lambda: d.demod(bursts)))
    assert d.frame_valid == 1, "the burst row must decode a CRC-valid frame"
    assert np.array_equal(out[:PAYLOAD], payload), (
        "payload did not come back bit-exact — this row would otherwise time "
        "a demodulator that had stopped working"
    )
    rate(benchmark)
