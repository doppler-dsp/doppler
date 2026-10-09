"""Benchmark for BerMeter — the two calls a BER sweep makes.

Run: pytest src/doppler/ber/benchmarks/bench_ber_meter.py --benchmark-only

Two rows over one 64k-symbol QPSK capture, because a sweep does not call
them equally often: ``align`` runs **once per capture** and ``score`` runs
**once per window**, so a sweep's cost is the score row times its point
count, plus one align. One blended figure would hide that.

- ``score`` — score all 64k symbols under the detected alignment.
- ``align`` — detect the alignment from a 256-symbol marker over a
  200-symbol lag span.

The capture is clean except for a sign flip every 997th symbol -- a pi
rotation, which on Gray-coded QPSK is one symbol error and two bit
errors -- so the scorer takes its error branch rather than a stream that
never does, and the row can assert the exact count it must find. A meter
that quietly stopped counting would otherwise just get faster.

The C benchmark (``native/benchmarks/bench_ber_meter_core.c``) times the
same two calls with the same geometry; the gap between the faces is the
binding's per-call overhead, which is what a Python benchmark is for.
"""

import numpy as np
import pytest

from doppler.ber import BerMeter

BLOCK_64K = 65_536
M = 4
FLIP_EVERY = 997
#: Marker geometry, the same as the C benchmark's align call.
T0, N_MARKER, LAG_SPAN = 1000, 256, 200


@pytest.fixture(scope="module")
def capture():
    """Truth indices and the received symbols, sign-flipped every 997th.

    The constellation is the one ``BerMeter.score``'s own doctest builds:
    index ``k`` at phase ``2*pi*k/M + pi/4``.
    """
    rng = np.random.default_rng(7)
    truth = rng.integers(0, M, size=BLOCK_64K).astype(np.uint8)
    rx = np.exp(1j * (2 * np.pi * truth / M + np.pi / 4)).astype(np.complex64)
    flips = np.arange(0, BLOCK_64K, FLIP_EVERY)
    rx[flips] *= -1
    return truth, rx, flips


@pytest.fixture
def meter(capture):
    truth, rx, _ = capture
    met = BerMeter(m=M, target_errors=200, conf=0.99)
    met.set_truth(truth)
    assert met.align(rx, T0, N_MARKER, 0, LAG_SPAN, 0.0) == 1
    return met


def test_bench_score(benchmark, meter, capture):
    """Score the whole capture; every flipped symbol outside the marker is
    exactly one symbol error and two bit errors."""
    truth, rx, flips = capture

    def fresh():
        # reset() clears the tallies and keeps the detected alignment, so
        # every round scores the same window from zero.
        meter.reset()
        meter.set_truth(truth)

    scored = benchmark.pedantic(
        meter.score,
        args=(rx, 0, BLOCK_64K),
        setup=fresh,
        rounds=50,
        warmup_rounds=2,
    )
    in_marker = (flips >= T0) & (flips < T0 + N_MARKER)
    assert scored == BLOCK_64K - N_MARKER
    assert meter.skipped == N_MARKER
    assert meter.errors == int((~in_marker).sum())
    assert meter.bit_errors == 2 * meter.errors
    if benchmark.stats:
        sec = benchmark.stats["min"]
        benchmark.extra_info["MSa_s"] = BLOCK_64K / sec / 1e6
        benchmark.extra_info["Msym_s"] = BLOCK_64K / sec / 1e6


def test_bench_align(benchmark, meter, capture):
    """Detect the alignment once; the capture is not delayed, so the lag
    found must be zero and the detection must clear its false-alarm gate."""
    truth, rx, _ = capture

    def fresh():
        meter.reset()
        meter.set_truth(truth)

    ok = benchmark.pedantic(
        meter.align,
        args=(rx, T0, N_MARKER, 0, LAG_SPAN, 0.0),
        setup=fresh,
        rounds=50,
        warmup_rounds=2,
    )
    assert ok == 1
    assert meter.align_ok
    assert meter.lag == 0
    if benchmark.stats:
        sec = benchmark.stats["min"]
        benchmark.extra_info["MSa_s"] = BLOCK_64K / sec / 1e6
        benchmark.extra_info["ms_per_capture"] = sec * 1e3
