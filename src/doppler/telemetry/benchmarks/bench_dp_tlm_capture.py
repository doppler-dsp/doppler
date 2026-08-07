"""Benchmark for the Python capture face.

What is measured here is the BOUNDARY, not the emit path — the C benchmark
(``native/benchmarks/bench_dp_tlm_capture_core.c``) owns the per-record cost
in isolation. The question this one answers is the one a Python caller
actually has: what does wrapping an existing block loop in a capture cost,
end to end, through the binding?

Run::

    pytest src/doppler/telemetry/benchmarks/bench_dp_tlm_capture.py \\
        --benchmark-only
"""

import numpy as np
import pytest

from doppler.agc import AGC
from doppler.telemetry import MemoryCapture, Telemetry
from doppler.wfm import SampleClock

BLOCK = 4096
BLOCKS = 16


@pytest.fixture
def signal():
    return (0.05 * np.ones(BLOCKS * BLOCK)).astype(np.complex64)


def _drive(agc, x, tlm=None):
    for i in range(0, len(x), BLOCK):
        if tlm is not None:
            tlm.set_now(i)
        agc.steps(x[i : i + BLOCK])


def test_bench_uninstrumented(benchmark, signal):
    """The floor: no telemetry context at all."""
    agc = AGC()
    benchmark(lambda: _drive(agc, signal))


def test_bench_attached_no_capture(benchmark, signal):
    """Probes attached and draining nowhere — the ring absorbs and drops."""
    tlm = Telemetry(1 << 16)
    agc = AGC()
    agc.set_telemetry(tlm, "agc", 1)
    benchmark(lambda: _drive(agc, signal, tlm))


def test_bench_captured(benchmark, signal):
    """The same loop made lossless: one memcpy per boundary, nothing more.

    The delta against `attached_no_capture` is the whole price of never
    dropping a record — which is the claim worth keeping honest.
    """

    def run():
        tlm = Telemetry(1 << 16)
        agc = AGC()
        agc.set_telemetry(tlm, "agc", 1)
        with MemoryCapture(tlm, BLOCK, SampleClock(1e6)) as cap:
            _drive(agc, signal, tlm)
            cap.close()

    benchmark(run)
