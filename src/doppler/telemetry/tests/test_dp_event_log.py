"""Integration tests for the ``EventLog`` binding.

The C tests (``native/tests/test_dp_event_log_core.c``) own the document's
content — which keys are omitted, what an edge is worth, that a staged field is
consumed by exactly one event. These own the *binding*: that the arguments
cross, that a refusal is a Python exception rather than a status code nobody
reads, and that the object behaves like a Python object (a context manager, a
property, ``os.PathLike``).
"""

import json
import math
from pathlib import Path

import pytest

from doppler.telemetry import EventLog


def _read(path):
    return json.loads(Path(path).read_text())


def test_pathlike_and_round_trip(tmp_path):
    """A Path, not just a str — and every argument reaching the document."""
    log = EventLog(tmp_path / "run.events", fc=2.4e9)
    log.field("emitter", 3)
    log.field("cn0_db_hz", 47.5)
    log.field_str("state", "tracking")
    log.append(48_000, "seeded", bandwidth_hz=4.0e6)
    log.append(96_000, "lost", sample_count=1024)
    assert log.count == 2

    log.set_dataset("capture.sigmf-data")
    log.set_telemetry("run.tlm")
    log.finalize(str(tmp_path / "run.sigmf-meta"), fs=1.0e7)
    log.close()

    meta = _read(tmp_path / "run.sigmf-meta")
    assert meta["global"]["core:datatype"] == "cf32_le"
    assert meta["global"]["core:sample_rate"] == 1.0e7
    assert meta["global"]["core:dataset"] == "capture.sigmf-data"
    assert meta["global"]["doppler:telemetry"]["path"] == "run.tlm"
    assert meta["captures"][0]["core:frequency"] == 2.4e9

    first, second = meta["annotations"]
    assert first["core:sample_start"] == 48_000
    assert first["core:label"] == "seeded"
    assert first["doppler:emitter"] == 3
    assert first["doppler:state"] == "tracking"
    assert first["core:freq_upper_edge"] == pytest.approx(2.4e9 + 2.0e6)
    # An instant omits the span; a stated one keeps it.
    assert "core:sample_count" not in first
    assert second["core:sample_count"] == 1024


def test_sample_type_and_endian_are_named_not_numbered(tmp_path):
    """The enum arguments are the strings the rest of the library uses."""
    log = EventLog(tmp_path / "run.events")
    log.append(0, "seeded")
    log.finalize(
        str(tmp_path / "run.sigmf-meta"), sample_type="ci16", endian="be"
    )
    log.close()
    meta = _read(tmp_path / "run.sigmf-meta")
    assert meta["global"]["core:datatype"] == "ci16_be"
    # Nothing was recorded and no rate was stated, so neither key is invented.
    assert meta["global"]["core:metadata_only"] is True
    assert "core:sample_rate" not in meta["global"]
    assert "core:frequency" not in meta["captures"][0]


def test_refusals_are_exceptions(tmp_path):
    """A refused field raises where C returns a status: a caller cannot
    silently keep going with a field that was never staged."""
    log = EventLog(tmp_path / "run.events")
    with pytest.raises(ValueError):
        log.field("cn0", math.nan)
    with pytest.raises(ValueError):
        log.field("x" * 64, 1.0)
    with pytest.raises(ValueError):
        log.field_str("state", "y" * 128)
    for i in range(16):
        log.field(f"f{i}", float(i))
    with pytest.raises(ValueError):
        log.field("one_too_many", 0.0)
    log.append(0, "seeded")
    log.close()


def test_context_manager_closes(tmp_path):
    """`with` exits through close(), so the events are on disk after it."""
    path = tmp_path / "run.events"
    with EventLog(path) as log:
        log.append(1, "seeded")
        log.append(2, "released")
    assert len(path.read_text().splitlines()) == 2


def test_append_to_a_closed_log_raises(tmp_path):
    log = EventLog(tmp_path / "run.events")
    log.close()
    with pytest.raises(OSError):
        log.append(1, "seeded")


def test_an_event_at_the_line_ceiling_raises(tmp_path):
    """The writer's half of the one line ceiling: an event that renders to
    16 KiB is refused and never counted, while a long label short of it is an
    ordinary event."""
    path = tmp_path / "run.events"
    with EventLog(path) as log:
        with pytest.raises(OSError):
            log.append(1, "x" * 16384)
        assert log.count == 0
        log.append(1, "x" * 1000)
        assert log.count == 1
    assert len(path.read_text().splitlines()) == 1


def test_unopenable_path_raises(tmp_path):
    with pytest.raises(OSError):
        EventLog(tmp_path / "no_such_dir" / "run.events")
