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


class TestTheClockIsNullable:
    """``clock=None`` states that there is no time base, and C means it.

    The C has always taken ``NULL`` here to mean "no time base stated", after
    which the sidecar omits the rate and epoch rather than fabricating them
    into a file that outlives the process. Only the binding refused to say it,
    until just-makeit 0.53.0 honoured ``required`` on a capsule init-param
    (gh-805 §H). These pin the two halves apart, because they are different
    axes and only one of them moved.
    """

    def test_none_is_accepted_and_the_capture_still_works(self):
        tlm, pid = _tlm()
        cap = MemoryCapture(tlm, BLOCK, None)
        tlm.emit(pid, 1.5)
        cap.close()
        recs = cap.records()
        assert len(recs) == 1
        assert recs[0]["value"] == pytest.approx(1.5)

    def test_a_real_clock_is_still_accepted_both_ways(self):
        clk = SampleClock(1e6)
        for arg in (clk, clk._capsule):
            tlm, _ = _tlm()
            MemoryCapture(tlm, BLOCK, arg).close()

    def test_the_argument_is_not_omittable(self):
        # Accepting None and being omittable are different axes, and only the
        # first ever moved. just-makeit#845 (0.53.1) resolved the divergence
        # from the STUB side: the .pyi used to render `clock: Any = ...` for
        # an argument the binding requires, and now renders
        # `clock: object | None` — a required positional. So this stayed green
        # across the fix, which is the point: the behaviour never changed, only
        # its published description caught up. The entry it justified is gone
        # from scripts/.init-param-optionality-ignore.
        tlm, _ = _tlm()
        with pytest.raises(TypeError, match="clock"):
            MemoryCapture(tlm, BLOCK)

    def test_file_flavour_omits_the_time_base_when_told_none(self, tmp_path):
        import json

        path = tmp_path / "cap.tlm16"
        tlm, pid = _tlm()
        cap = Capture(tlm, BLOCK, path, None)
        tlm.emit(pid, 2.0)
        cap.close()
        meta = json.loads((tmp_path / "cap.tlm16-meta").read_text())
        # Not fabricated: absent, not a confident zero.
        assert "fs" not in meta
        assert "epoch_real_ns" not in meta


class TestReadDict:
    """The capture's records, grouped by probe name.

    The split itself is C (``dp_tlm_demux``); what is checked here is that the
    capture resolves ids against the context it was opened on, and that —
    unlike ``Telemetry.read_dict`` — reading does not consume.
    """

    def test_groups_the_capture_by_name(self):
        tlm = Telemetry(1 << 12)
        a = tlm.probe("rx.a")
        b = tlm.probe("rx.b")
        with MemoryCapture(tlm, BLOCK, SampleClock(1e6)) as cap:
            for i in range(4):
                tlm.set_now(i * BLOCK)
                tlm.emit(a, float(i))
                tlm.emit(b, float(-i))
            cap.close()
            d = cap.read_dict()

        assert sorted(d) == ["rx.a", "rx.b"]
        assert [float(v) for v in d["rx.a"]] == [0.0, 1.0, 2.0, 3.0]
        assert [float(v) for v in d["rx.b"]] == [0.0, -1.0, -2.0, -3.0]

    def test_index_gives_the_boundary_stamp(self):
        """`n` is what makes a real time axis possible: seconds = n / fs."""
        tlm = Telemetry(1 << 12)
        p = tlm.probe("rx.x")
        with MemoryCapture(tlm, BLOCK, SampleClock(1e6)) as cap:
            for i in range(3):
                tlm.set_now(i * BLOCK)
                tlm.emit(p, float(i))
            cap.close()
            n, v = cap.read_dict(index=True)["rx.x"]

        assert [int(x) for x in n] == [0, BLOCK, 2 * BLOCK]
        assert [float(x) for x in v] == [0.0, 1.0, 2.0]

    def test_it_does_not_consume(self):
        """records() is re-readable, and so is this: the same data."""
        tlm = Telemetry(1 << 12)
        p = tlm.probe("rx.x")
        with MemoryCapture(tlm, BLOCK, SampleClock(1e6)) as cap:
            _run(tlm, p, cap, blocks=3)
            cap.close()
            assert cap.read_dict()["rx.x"].size == 3
            assert cap.read_dict()["rx.x"].size == 3
            assert cap.records().size == 3

    def test_n_takes_from_the_front(self):
        tlm = Telemetry(1 << 12)
        p = tlm.probe("rx.x")
        with MemoryCapture(tlm, BLOCK, SampleClock(1e6)) as cap:
            _run(tlm, p, cap, blocks=5)
            cap.close()
            assert cap.read_dict(2)["rx.x"].size == 2
            assert cap.read_dict()["rx.x"].size == 5

    def test_a_silent_probe_still_gets_a_key(self):
        tlm = Telemetry(1 << 12)
        p = tlm.probe("rx.loud")
        tlm.probe("rx.quiet")
        with MemoryCapture(tlm, BLOCK, SampleClock(1e6)) as cap:
            _run(tlm, p, cap, blocks=2)
            cap.close()
            d = cap.read_dict()

        assert sorted(d) == ["rx.loud", "rx.quiet"]
        assert d["rx.quiet"].size == 0

    def test_the_file_flavour_has_no_read_dict(self):
        """It follows records(): the file IS the capture, nothing to group."""
        assert not hasattr(Capture, "read_dict")
        assert hasattr(MemoryCapture, "read_dict")


class TestExitFinalizesRatherThanFrees:
    """`with` exit runs close(), not destroy() — gh-805 §H.

    A capture's records and its drop verdict only become valid once the tail
    is drained, so freeing at block exit discarded the object at exactly the
    moment it became worth reading. `exit = "close"` splits the two: the
    block finalizes, `tp_dealloc` still frees exactly once.
    """

    def test_records_survive_the_block(self):
        tlm, pid = _tlm()
        with MemoryCapture(tlm, BLOCK, SampleClock(1e6)) as cap:
            _run(tlm, pid, cap, blocks=4)
        # No explicit close(), and read AFTER the block — both were impossible
        # while __exit__ freed.
        assert cap.records().size == 4
        assert cap.read_dict()["bench.x"].size == 4
        assert cap.count == 4
        assert cap.dropped == 0

    def test_the_verdict_still_raises_on_exit(self):
        """Splitting the calls must not cost the loud failure."""
        tlm, pid = _tlm()
        with (
            pytest.raises(ValueError, match="hole"),
            MemoryCapture(tlm, 8, SampleClock(1e6)),
        ):
            _make_a_hole(tlm, pid)

    def test_an_explicit_close_is_still_fine(self):
        """close() is idempotent, so the old shape keeps working."""
        tlm, pid = _tlm()
        with MemoryCapture(tlm, BLOCK, SampleClock(1e6)) as cap:
            _run(tlm, pid, cap, blocks=3)
            cap.close()
        assert cap.records().size == 3

    def test_every_teardown_path_reports_the_same_verdict(self):
        """close(), destroy() and `with` exit are one condition, three routes.

        The teardown states no error of its own and inherits the finalizer's,
        so a caller learns exactly as much from letting the object fall out of
        scope as from asking.
        """
        for teardown in (
            lambda c: c.close(),
            lambda c: c.destroy(),
        ):
            tlm, pid = _tlm()
            cap = MemoryCapture(tlm, 8, SampleClock(1e6))
            _make_a_hole(tlm, pid)
            with pytest.raises(ValueError, match="block bound"):
                teardown(cap)
