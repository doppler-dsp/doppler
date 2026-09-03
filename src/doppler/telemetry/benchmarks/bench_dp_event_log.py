"""Benchmark for the Python event-log face.

The C benchmark (``native/benchmarks/bench_dp_event_log_core.c``) owns the
per-event cost of the render, the write and the flush. This one answers the
question a Python caller has instead: what does the *binding* add, and is a
transition still cheap enough to write from the thread that is pushing
samples?

Run::

    pytest src/doppler/telemetry/benchmarks/bench_dp_event_log.py \\
        --benchmark-only
"""

import pytest

from doppler.telemetry import EventLog

EVENTS = 256


@pytest.fixture
def log(tmp_path):
    ev = EventLog(tmp_path / "bench.events", fc=2.4e9)
    yield ev
    ev.close()


def _bare(log):
    for n in range(EVENTS):
        log.append(n * 4096, "tracking", bandwidth_hz=4.0e6, freq_hz=1.0e4)


def _with_fields(log):
    for n in range(EVENTS):
        log.field("emitter", 3)
        log.field("cn0_db_hz", 47.5)
        log.field("doppler_hz", 1234.5)
        log.field("code_phase_chips", 511.25)
        log.field_str("state", "tracking")
        log.append(n * 4096, "tracking", bandwidth_hz=4.0e6, freq_hz=1.0e4)


def test_bench_append(benchmark, log):
    """A labelled event with a band and nothing else — the floor."""
    benchmark(lambda: _bare(log))


def test_bench_append_with_fields(benchmark, log):
    """The shape a receiver transition actually has: five staged fields.

    The difference from ``test_bench_append`` is what the staging table and
    its five binding round-trips cost, which is the part a caller can choose.
    """
    benchmark(lambda: _with_fields(log))


def test_bench_finalize(benchmark, log, tmp_path):
    """Rendering the sidecar: read the flat file back, build the document.

    Per FINALIZE, not per event — a long run calls this occasionally, and the
    cost grows with the events already written, which is why it is measured
    over a log that has some.
    """
    _bare(log)
    meta = str(tmp_path / "bench.sigmf-meta")
    benchmark(lambda: log.finalize(meta, fs=1.0e7))
