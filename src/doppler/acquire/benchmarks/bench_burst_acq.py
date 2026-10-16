"""Benchmark for BurstAcquisition — the burst chain's search stage.

Run: pytest src/doppler/acquire/benchmarks/bench_burst_acq.py --benchmark-only

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

from doppler.acquire import BurstAcquisition
from doppler.acquire.tests._preamble import code_preamble
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
    return BurstAcquisition(
        code_preamble(acq_code, SPC),
        REPS,
        CHIP_RATE * SPC,
        60.0,
        0.0,
        1e-3,
        0.9,
        "mean",
    )


def test_bench_construct(benchmark, waveform):
    """Construction: the search sized on the burst, paid once per object."""
    acq_code = waveform[0]
    benchmark(lambda: _acq(acq_code))
