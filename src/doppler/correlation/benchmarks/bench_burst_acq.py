"""Benchmark for the burst search (Correlator, framing="burst").

The burst chain's search stage.

Run: make bench

One row, ``construct``: the search sized on the burst, paid once per object.
Anyone who builds per channel, per retune or a pool of receivers pays it,
and no C row times construction.

The per-sample rows (``push_idle``, ``push_bursts``) were cut (doppler#2386):
the binding adds nothing a user needs over the C ``burst_acq::push`` rows,
which time the code period's two ends, and a detection added nothing to the
cost (56.5 against 56.9 MSa/s). Whether a block with bursts in it detects
is this module's tests' to say, not a page row's.
"""

import pytest

from doppler.acquire.tests._preamble import code_preamble
from doppler.correlation import Correlator
from doppler.dsss.benchmarks._burst_stimulus import (
    CHIP_RATE,
    REPS,
    SPC,
    burst_stimulus,
)


@pytest.fixture(scope="module")
def waveform():
    return burst_stimulus()


def _acq(acq_code):
    return Correlator(
        code_preamble(acq_code, SPC),
        framing="burst",
        reps=REPS,
        sample_rate=CHIP_RATE * SPC,
        cn0_dbhz=60.0,
        doppler_uncertainty=0.0,
        pfa=1e-3,
        pd=0.9,
        noise_mode="mean",
    )


def test_bench_construct(benchmark, waveform):
    """Construction: the search sized on the burst, paid once per object."""
    acq_code = waveform[0]
    benchmark(lambda: _acq(acq_code))
