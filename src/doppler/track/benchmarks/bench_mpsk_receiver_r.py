"""Benchmarks for MpskReceiverR — throughput, and the cost of telemetry.

Run: pytest src/doppler/track/benchmarks/bench_mpsk_receiver_r.py \
     --benchmark-only

The real-IF twin of `bench_mpsk_receiver.py`; see that file for why the
telemetry pair is measured as two separately-constructed receivers rather than
one toggled in place (a naive A-then-B ordering charges the lazy output-buffer
allocation to whichever runs first and reports telemetry as *faster*).

The IF sits at the design centre `fs/4`, where the R2C halfband's +fs/4 shift
makes the front end symmetric and its image rejection best. Benchmarking off
that centre would fold a placement penalty into a throughput number.
"""

import numpy as np
import pytest

from doppler.telemetry import Telemetry
from doppler.track import MpskReceiverR

BLOCK_64K = 65_536
SPS = 16
M_OUT = 4
FC = 0.25  # the design centre
# 11 probes x (65536/16) symbols = 45056 records; size the ring past that
# so the measurement is a ring WRITE, not a wrap or a drop.
RING = 1 << 18


@pytest.fixture
def rx():
    """A real QPSK IF at fs/4 — the geometry the front end is designed for."""
    rng = np.random.default_rng(0)
    nsym = BLOCK_64K // SPS
    syms = np.exp(2j * np.pi * rng.integers(0, 4, nsym) / 4)
    bb = np.repeat(syms, SPS) * 0.5
    n = np.arange(bb.size)
    return (bb * np.exp(2j * np.pi * FC * n)).real.astype(np.float32)


def _receiver():
    """Keyword-constructed, and nothing ever ran this file before: `bench_*.py`
    is outside pytest's default discovery and the scaffold had no benchmark
    function at all, only a fixture."""
    return MpskReceiverR(
        m=4,
        sps=float(SPS),
        m_out=M_OUT,
        pulse="iandd",
        bn_carrier=0.01,
        bn_timing=0.01,
        init_norm_freq=FC,
    )


@pytest.fixture
def obj():
    return _receiver()


def test_bench_steps_64k(benchmark, obj, rx):
    """Baseline throughput: 64k real samples, no telemetry attached."""
    benchmark(obj.steps, rx)


def test_bench_steps_64k_telemetry_detached(benchmark, rx):
    """The A half of the telemetry pair — explicitly detached."""
    obj = _receiver()
    obj.set_telemetry(None, "rx")
    benchmark(obj.steps, rx)


def test_bench_steps_64k_telemetry_attached(benchmark, rx):
    """The B half — all eleven probes writing, ring drained each pass."""
    obj = _receiver()
    tlm = Telemetry(RING)
    obj.set_telemetry(tlm, "rx")

    def run():
        obj.steps(rx)
        tlm.read()

    benchmark(run)
