"""Benchmark for Telemetry — the emit path, decimated and not, and the drain.

Run: pytest src/doppler/telemetry/benchmarks/bench_dp_tlm.py --benchmark-only

The C benchmark (``native/benchmarks/bench_dp_tlm_core.c``) times
``dp_tlm_emit`` inlined into a C loop. This face cannot: Python's ``emit``
is the VALIDATING out-of-line twin (it refuses an unregistered id), called
once per event across the binding — so these rows are the price of
instrumenting from Python, which is a different number by design, and the
gap to the C rows is that per-call cost.

- ``emit``      — decim 1: 64k events, every one written to the ring.
- ``decimated`` — decim 16: the same 64k events, 1 in 16 written. Split from
  ``emit`` because in C the two branches do not cost the same. From Python
  they come out level (~31 M events/s each, measured on an aarch64 box):
  the ~30 ns binding call swamps the ~1 ns write it would skip, so from
  this face decimation saves ring space, not time. That is the finding the
  pair exists to show; if the rows ever part, the binding got cheaper.
- ``read``      — the consumer: drain 64k records into a caller-owned
  structured array. Block-shaped, so this row measures the copy rather than
  the binding.

The C ``detached`` and ``full`` arms have no Python counterpart: a NULL
context is not constructible from Python, and an overrunning ring is a
fault to avoid rather than a rate to quote.

The ring is 128k records — larger than one round — and each emit round
starts drained (``pedantic``'s untimed setup), so ``emit`` times the write
and never the overrun branch.
"""

import numpy as np
import pytest

from doppler.telemetry import Telemetry

BLOCK_64K = 65_536
RING = 1 << 17
ROUNDS = 50
NAME = "bench.x"


@pytest.fixture(scope="module")
def values():
    # Python floats, built once: the row times emit, not float().
    return [float(i) for i in range(BLOCK_64K)]


def _drain(tlm):
    tlm.read(0)


def _emit_row(benchmark, values, decim):
    tlm = Telemetry(ring_records=RING)
    pid = tlm.probe(NAME, decim)
    emit = tlm.emit

    def run():
        for v in values:
            emit(pid, v)

    benchmark.pedantic(run, setup=lambda: _drain(tlm), rounds=ROUNDS)

    # One round is in the ring (setup drained the ones before it).
    want = BLOCK_64K // decim
    assert tlm.avail == want, (
        f"decim {decim}: {tlm.avail} records after one round, not {want} — "
        "a probe that stopped writing just looks faster"
    )
    assert tlm.dropped == 0, "the ring overran: this timed the drop branch"
    recs = tlm.read(0)
    np.testing.assert_array_equal(
        recs["value"], np.asarray(values[::decim], dtype=np.float32)
    )

    if benchmark.stats:
        # MIN, not mean: the least-disturbed observation, per
        # docs/dev/contributing/benchmarking.md. Credited with the events
        # OFFERED — the decimated row's point is what skipping costs.
        sec = benchmark.stats["min"]
        benchmark.extra_info["MSa_s"] = BLOCK_64K / sec / 1e6


def test_bench_emit(benchmark, values):
    """Every event written: decim 1."""
    _emit_row(benchmark, values, 1)


def test_bench_decimated(benchmark, values):
    """1 in 16 written: the skip branch, which is most events."""
    _emit_row(benchmark, values, 16)


def test_bench_read(benchmark, values):
    """Drain 64k records into a caller-owned buffer."""
    tlm = Telemetry(ring_records=RING)
    pid = tlm.probe(NAME, 1)
    out = np.empty(BLOCK_64K, dtype=tlm.read(1).dtype)

    def fill():
        for v in values:
            tlm.emit(pid, v)

    got = benchmark.pedantic(
        tlm.read,
        args=(BLOCK_64K,),
        kwargs={"out": out},
        setup=fill,
        rounds=ROUNDS,
    )

    assert got.shape == (BLOCK_64K,), (
        f"read returned {got.shape[0]} of {BLOCK_64K} records"
    )
    np.testing.assert_array_equal(
        got["value"], np.asarray(values, dtype=np.float32)
    )
    assert tlm.avail == 0

    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["min"] / 1e6
        )
