"""Benchmarks for MpskReceiver — throughput, and the cost of telemetry.

Run: pytest src/doppler/track/benchmarks/bench_mpsk_receiver.py \
     --benchmark-only

The telemetry pair is the point of this file. Both receivers publish eleven
probes, and the question a caller actually asks is "what does leaving them
attached cost me?" — one 16-byte ring write per probe per symbol. Measuring it
naively gets the SIGN wrong: with no warm-up and an A-then-B ordering, the lazy
first-call allocation of the output buffer is charged to whichever variant runs
first, and telemetry-attached measured 43% *faster* than detached.
pytest-benchmark's own warm-up plus a separate receiver per variant avoid
that here. The honest figure is ~+10%: measured 1.3237 ms detached vs
1.4630 ms attached (min of 746/552 rounds), with the independently
constructed no-telemetry baseline agreeing with the detached variant to
within 0.4% -- which is what says the pairing itself is sound.
"""

import numpy as np
import pytest

from doppler.telemetry import Telemetry
from doppler.track import MpskReceiver

BLOCK_64K = 65_536
SPS = 8
M_OUT = 4
# 11 probes x (65536/8) symbols = 90112 records; size the ring well past
# that, so the measurement is a ring WRITE, not a wrap or a drop.
RING = 1 << 18


@pytest.fixture
def rx():
    """A QPSK block with a small carrier offset, so both loops do real work."""
    rng = np.random.default_rng(0)
    nsym = BLOCK_64K // SPS
    syms = np.exp(2j * np.pi * rng.integers(0, 4, nsym) / 4)
    sig = np.repeat(syms.astype(np.complex64), SPS) * 0.5
    k = np.arange(len(sig))
    return (sig * np.exp(2j * np.pi * 0.0005 * k)).astype(np.complex64)


def _receiver():
    """Keyword-constructed on purpose: this file previously passed the pulse
    string as the first positional argument, i.e. as `m`, and `bench_*.py` is
    outside pytest's default discovery so nothing ever ran it."""
    return MpskReceiver(
        m=4,
        sps=float(SPS),
        m_out=M_OUT,
        pulse="iandd",
        bn_carrier=0.01,
        bn_timing=0.01,
        init_norm_freq=0.0,
    )


@pytest.fixture
def obj():
    return _receiver()


def test_bench_steps_64k(benchmark, obj, rx):
    """Baseline throughput: 64k samples, no telemetry attached."""
    benchmark(obj.steps, rx)


def test_bench_steps_64k_telemetry_detached(benchmark, rx):
    """The A half of the telemetry pair — an explicitly detached receiver.

    Deliberately a separate measurement from `test_bench_steps_64k`: this one
    exists so the pair is apples-to-apples, constructed the same way and
    differing only in whether the probes are attached.
    """
    obj = _receiver()
    obj.set_telemetry(None, "rx")
    benchmark(obj.steps, rx)


def test_bench_steps_64k_telemetry_attached(benchmark, rx):
    """The B half — eleven probes writing, ring drained each pass.

    The drain is part of the measurement on purpose: letting the ring fill
    would measure dropped records rather than the write cost a caller with a
    real consumer actually pays.
    """
    obj = _receiver()
    tlm = Telemetry(RING)
    obj.set_telemetry(tlm, "rx")

    def run():
        obj.steps(rx)
        tlm.read()

    benchmark(run)
