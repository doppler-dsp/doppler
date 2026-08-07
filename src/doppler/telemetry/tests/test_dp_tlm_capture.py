"""Integration tests for the Python face of the lossless capture.

The C suite (``native/tests/test_dp_tlm_capture_core.c``) owns the losslessness
proof itself — the saturation test that emits exactly the per-block bound and
asserts nothing is dropped. What is checked HERE is everything the binding
adds on top of it, because none of it exists in C:

* the two flavours and what distinguishes them (``MemoryCapture`` can hand its
  records back; ``Capture`` writes a file and therefore cannot),
* the record dtype matching the C layout byte for byte, which is what lets
  ``np.fromfile`` read a capture with no doppler code at all, and
* **the hole raising on every exit path**, which is the whole reason
  ``close()`` reports a verdict rather than returning quietly.
"""

import numpy as np
import pytest

from doppler.telemetry import Capture, MemoryCapture, Telemetry
from doppler.wfm import SampleClock

BLOCK = 64


def _tlm():
    """A context with exactly one probe, so the block bound is 1 * BLOCK."""
    tlm = Telemetry(1 << 12)
    return tlm, tlm.probe("bench.x")


def _run(tlm, pid, cap, blocks=8, per_block=1):
    """Drive a well-behaved block loop: stamp the boundary, then emit."""
    for b in range(blocks):
        tlm.set_now(b * BLOCK)
        for _ in range(per_block):
            tlm.emit(pid, float(b))


def test_memory_capture_round_trips_every_record():
    tlm, pid = _tlm()
    with MemoryCapture(tlm, BLOCK, SampleClock(1e6)) as cap:
        _run(tlm, pid, cap)
        cap.close()
        recs = cap.records()

    assert len(recs) == 8
    assert [float(v) for v in recs["value"]] == [float(b) for b in range(8)]
    # `n` is the boundary stamp in force when the record was emitted.
    assert [int(n) for n in recs["n"]] == [b * BLOCK for b in range(8)]


def test_records_dtype_is_the_c_layout():
    """16 bytes with C's offsets — not numpy's packing of the field list.

    This is the property that makes the file self-describing: get it wrong and
    every row after the first is read misaligned.
    """
    tlm, pid = _tlm()
    with MemoryCapture(tlm, BLOCK, SampleClock(1e6)) as cap:
        _run(tlm, pid, cap)
        cap.close()
        dt = cap.records().dtype

    assert dt.itemsize == 16
    assert dt.names == ("n", "value", "probe", "flags")
    assert [dt.fields[f][1] for f in dt.names] == [0, 8, 12, 14]


def test_records_honours_a_request_smaller_than_the_capture():
    tlm, pid = _tlm()
    with MemoryCapture(tlm, BLOCK, SampleClock(1e6)) as cap:
        _run(tlm, pid, cap)
        cap.close()
        assert cap.records(3).shape == (3,)
        # 0 means "everything", and the drain is not consuming.
        assert cap.records().shape == (8,)


def test_file_capture_is_readable_with_numpy_alone(tmp_path):
    """The 16-byte record layout IS the file: no framing, no header."""
    tlm, pid = _tlm()
    path = tmp_path / "rx.tlm"
    with Capture(tlm, BLOCK, path, SampleClock(1e6)) as cap:
        _run(tlm, pid, cap)

    dtype = np.dtype(
        {
            "names": ["n", "value", "probe", "flags"],
            "formats": ["<u8", "<f4", "<u2", "<u2"],
            "offsets": [0, 8, 12, 14],
            "itemsize": 16,
        }
    )
    got = np.fromfile(path, dtype=dtype)
    assert [float(v) for v in got["value"]] == [float(b) for b in range(8)]
    assert (path.parent / (path.name + "-meta")).exists()


def test_file_flavour_has_no_records_accessor():
    """The file IS the capture, so there is nothing to hand back.

    An AttributeError says that; an empty array would read as "nothing was
    captured", which is a different and wrong statement.
    """
    assert not hasattr(Capture, "records")
    assert hasattr(MemoryCapture, "records")


def test_capture_accepts_a_path_like(tmp_path):
    tlm, pid = _tlm()
    with Capture(tlm, BLOCK, str(tmp_path / "s.tlm"), SampleClock(1e6)):
        _run(tlm, pid, None)
    assert (tmp_path / "s.tlm").exists()


# ── the invariant: a hole is never silent ───────────────────────────────────
#
# Breaking the block contract is the ONLY way to lose a record, so these drive
# it deliberately: emit far past the bound with no boundary at all, and the
# ring — sized to exactly one block — must overflow.


def _make_a_hole(tlm, pid):
    for i in range(20_000):
        tlm.emit(pid, float(i))


def test_close_raises_on_a_hole():
    tlm, pid = _tlm()
    cap = MemoryCapture(tlm, 8, SampleClock(1e6))
    _make_a_hole(tlm, pid)
    assert tlm.dropped > 0
    with pytest.raises(ValueError):
        cap.close()


def test_context_manager_exit_raises_on_a_hole():
    """The path a user actually takes, and the one a verdict could slip out of.

    `with` exit runs the destructor, and a destructor that discarded the close
    verdict would make the loud failure unreachable from idiomatic Python.
    """
    tlm, pid = _tlm()
    with (
        pytest.raises(ValueError, match="hole"),
        MemoryCapture(tlm, 8, SampleClock(1e6)),
    ):
        _make_a_hole(tlm, pid)


def test_a_clean_capture_exits_silently():
    """The other half of the mutation test: no false alarm on a good run."""
    tlm, pid = _tlm()
    with MemoryCapture(tlm, BLOCK, SampleClock(1e6)) as cap:
        _run(tlm, pid, cap)
    assert tlm.dropped == 0


def test_dropped_is_latched_to_this_capture():
    """Not the context's lifetime counter — this capture's own losses."""
    tlm, pid = _tlm()
    _make_a_hole(tlm, pid)  # damage BEFORE the capture exists
    before = tlm.dropped
    assert before > 0
    tlm.read()  # drain, so the ring starts empty

    with MemoryCapture(tlm, BLOCK, SampleClock(1e6)) as cap:
        _run(tlm, pid, cap)
        cap.close()
        assert cap.dropped == 0  # this capture lost nothing


def test_count_tracks_records_captured():
    tlm, pid = _tlm()
    with MemoryCapture(tlm, BLOCK, SampleClock(1e6)) as cap:
        _run(tlm, pid, cap, blocks=5)
        cap.close()
        assert cap.count == 5


def test_opening_without_probes_explains_itself():
    """The bound comes from the probe table, so no probes means no bound.

    A blanket MemoryError would misexplain this completely, which is what
    create_error exists to prevent.
    """
    tlm = Telemetry(1 << 12)  # no probe() call
    with pytest.raises(ValueError, match="probe"):
        MemoryCapture(tlm, BLOCK, SampleClock(1e6))


def test_second_capture_on_one_context_is_refused():
    tlm, _pid = _tlm()
    with (
        MemoryCapture(tlm, BLOCK, SampleClock(1e6)),
        pytest.raises(ValueError),
    ):
        MemoryCapture(tlm, BLOCK, SampleClock(1e6))
