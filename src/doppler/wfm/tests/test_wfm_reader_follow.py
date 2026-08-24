"""The follow read, from the face a Python caller uses.

`docs/design/end-of-capture.md` is a contract about *waiting*: a reader
following a capture that is still being written must tell "nothing yet" apart
from "the end", and must be stoppable while it waits. All of that was pinned
in C (`native/tests/test_wfm_reader_core.c`) and none of it in Python, which
is phase 5 of that design's own lifecycle table -- the binding, and the stop
path through it.

The stop path could not work before doppler#976 was fixed. `read_follow()`'s
predicate is `dp_interrupted`, installed by the binding, and the flag
`Interrupt()` sets lives in `doppler.interrupt` -- a different `.so`. Until
`dp_interrupt_guard` was declared `process_global` those were two different
variables, so this file would have hung rather than failed. Every blocking
wait here therefore runs in a thread with a join deadline: a regression
FAILS the suite instead of wedging it.

The three rules under test are the ones the design says fall out, each
phrased as what goes wrong when it is dropped:

- **drain outranks the stop** -- or Ctrl+C discards a tail already safely on
  disk;
- **the reader's normal exit is the marker, not the flag** -- or it races the
  shutdown instead of joining it;
- **both budgets are unbounded by default** -- so a finite one is something
  the caller asks for, and a stop with no grace waits for the marker forever
  ON PURPOSE.
"""

from __future__ import annotations

import contextlib
import threading

import numpy as np
import pytest

from doppler.interrupt import Interrupt
from doppler.wfm import Reader, Writer

#: Long enough that a loaded machine is not a failure, short enough that a
#: genuine hang is reported rather than waited out.
_JOIN_S = 10.0

#: The grace a stopped reader is given to see the writer's marker. Non-zero
#: on purpose: 0 means "forever", which is the DEFAULT and is what makes the
#: shutdown propagate through the file rather than around it.
_GRACE_MS = 200


@pytest.fixture(autouse=True)
def _clear_the_flag():
    """The interrupt is process-wide, and since doppler#976 it really is.

    A test that leaves it set would make the next module's blocking wait
    return instantly, so this puts it back. Constructing a guard also clears
    it, which is why the setup half is a construction rather than a call.
    """
    Interrupt(np.array([], dtype=np.int32)).resume()
    yield
    Interrupt(np.array([], dtype=np.int32)).resume()


def _capture(path, n=4096):
    """An OPEN capture with `n` samples flushed and no end marker yet."""
    w = Writer(path, file_type="blue", sample_type="ci16", fs=2.4e6)
    w.write(np.zeros(n, dtype=np.complex64))
    w.flush()
    return w


def _in_thread(fn):
    """Run `fn` and return its value, failing rather than hanging."""
    box: dict[str, object] = {}

    def run():
        try:
            box["v"] = fn()
        except BaseException as e:
            box["e"] = e

    t = threading.Thread(target=run, daemon=True)
    t.start()
    t.join(timeout=_JOIN_S)
    if t.is_alive():
        pytest.fail(
            f"the follow read never came back within {_JOIN_S}s — it is not "
            f"seeing the stop. If this is new, check that doppler.wfm still "
            f"joins the interrupt rendezvous (doppler#976): see "
            f"src/doppler/tests/test_interrupt_is_process_wide.py"
        )
    if "e" in box:
        raise box["e"]  # type: ignore[misc]
    return box["v"]


def test_a_stop_ends_a_blocked_follow_read(tmp_path):
    """The claim phase 5 exists to make, across two extension modules.

    `Interrupt` is `doppler.interrupt`; the wait is inside `doppler.wfm`.
    """
    p = tmp_path / "stop.blue"
    w = _capture(p)
    try:
        it = Interrupt(np.array([], dtype=np.int32))
        r = Reader(p)
        r.follow_grace_ms = _GRACE_MS
        assert len(r.read_follow(4096)) == 4096  # drain what is there

        # Requested from THIS thread while the read blocks in the other one.
        timer = threading.Timer(0.2, it.interrupt)
        timer.start()
        try:
            got = _in_thread(lambda: r.read_follow(4096))
        finally:
            timer.join()

        assert len(got) == 0
        assert r.ending == "interrupted"
    finally:
        w.close()


def test_drain_outranks_the_stop(tmp_path):
    """Samples already on disk come back even after a stop is requested.

    Otherwise Ctrl+C discards a tail that was safely written, which is the
    one outcome an end-of-capture contract must never produce.
    """
    p = tmp_path / "drain.blue"
    w = _capture(p, n=4096)
    try:
        it = Interrupt(np.array([], dtype=np.int32))
        r = Reader(p)
        r.follow_grace_ms = _GRACE_MS
        it.interrupt()  # stop FIRST, with data still unread

        got = _in_thread(lambda: r.read_follow(4096))
        assert len(got) == 4096, "a requested stop discarded a readable tail"
        assert r.ending == "none"
    finally:
        w.close()


def test_the_normal_ending_is_the_marker_not_the_flag(tmp_path):
    """A closed capture ends the read as EOF, with nothing interrupted.

    This is the path a real shutdown takes: the stop reaches the WRITER, the
    writer closes, and `close()` patching the BLUE header's `data_size` is
    what the reader ends on.
    """
    p = tmp_path / "eof.blue"
    w = _capture(p)
    r = Reader(p)
    r.follow_grace_ms = _GRACE_MS
    assert len(r.read_follow(4096)) == 4096
    w.close()  # the marker lands

    got = _in_thread(lambda: r.read_follow(4096))
    assert len(got) == 0
    assert r.ending == "eof"


def test_a_finite_budget_is_something_the_caller_asks_for(tmp_path):
    """`follow_timeout_ms` bounds the wait; the default does not.

    The default is 0 = forever, deliberately: a stream whose rhythm doppler
    does not control turns any finite budget into a spurious ending. A caller
    that knows its own rate can still ask for one.
    """
    p = tmp_path / "budget.blue"
    w = _capture(p)
    try:
        r = Reader(p)
        assert r.follow_timeout_ms == 0, "the default must be unbounded"
        assert r.follow_grace_ms == 0

        r.follow_timeout_ms = _GRACE_MS
        assert len(r.read_follow(4096)) == 4096

        got = _in_thread(lambda: r.read_follow(4096))
        assert len(got) == 0
        assert r.ending == "timeout"
    finally:
        w.close()


def test_a_zero_length_ask_returns_at_once(tmp_path):
    """`read_follow(0)` is answered, not waited on.

    A budget of "forever" plus an ask of nothing would otherwise be a wait
    that can never be satisfied, which is the one shape this contract must
    not produce by accident.
    """
    p = tmp_path / "zero.blue"
    w = _capture(p)
    try:
        r = Reader(p)
        got = _in_thread(lambda: r.read_follow(0))
        assert len(got) == 0
        assert r.ending == "none"
    finally:
        w.close()


def test_an_undersized_out_is_refused_not_truncated(tmp_path):
    """A short `out=` raises rather than quietly returning less.

    Written expecting the opposite -- that the smaller of `count` and `out`
    would win -- and the binding corrected it: `read_follow` sizes against
    `read_follow_max_out(count)` and refuses anything shorter. That is the
    better contract and the reason is the asymmetry of the two mistakes. A
    caller who passes a buffer too small has made an error they can fix; a
    read that silently returns 64 of the 4096 samples they asked for looks
    exactly like "that is all there was", which on a FOLLOW read is the one
    thing this whole design exists to tell apart.
    """
    p = tmp_path / "small.blue"
    w = _capture(p, n=4096)
    try:
        r = Reader(p)
        need = r.read_follow_max_out(4096)
        assert need >= 4096

        buf = np.zeros(64, dtype=np.complex64)
        with pytest.raises(ValueError, match="need >="):
            r.read_follow(4096, out=buf)

        # And the correctly-sized buffer is accepted, so the guard is a
        # bound and not a blanket refusal of `out=`.
        big = np.zeros(need, dtype=np.complex64)
        got = _in_thread(lambda: r.read_follow(4096, out=big))
        assert len(got) == 4096
        assert np.array_equal(got, big[:4096])
    finally:
        w.close()


def test_a_raw_capture_never_ends_so_only_a_stop_finishes_it(tmp_path):
    """Raw and CSV never report an ending, and it is not for lack of room.

    BLUE patches `data_size` at close() and that transition is the ending.
    Raw and CSV could carry a marker -- they already get a
    `<path>.sigmf-meta` sidecar for the fs/fc/t0 the containers cannot hold
    -- but a sidecar is a SECOND FILE: opt-out (`sidecar=false`), and
    separable from its data by a move, a copy or a glob. Writing the marker
    inline is worse still: raw has no framing, so it would be read as
    samples.

    So the ending stays unavailable rather than becoming best-effort, which
    is the whole basis for `DP_ERR_EOF` being a guarantee on BLUE: the
    marker is in the artifact the reader is already reading. A marker that
    usually arrives would be trusted, and would then be wrong exactly when
    the sidecar went missing.

    The only way out of this wait is a stop -- which is precisely why the
    stop path is not optional.
    """
    p = tmp_path / "nomarker.raw"
    w = Writer(p, file_type="raw", sample_type="ci16", fs=2.4e6)
    w.write(np.zeros(4096, dtype=np.complex64))
    w.flush()
    try:
        it = Interrupt(np.array([], dtype=np.int32))
        r = Reader(p, sample_type="ci16")
        r.follow_grace_ms = _GRACE_MS
        assert len(r.read_follow(4096)) == 4096

        w.close()  # closing a RAW capture writes no marker
        timer = threading.Timer(0.2, it.interrupt)
        timer.start()
        try:
            got = _in_thread(lambda: r.read_follow(4096))
        finally:
            timer.join()

        assert len(got) == 0
        assert r.ending == "interrupted", (
            "a raw capture reported an ending it has no way to know"
        )
    finally:
        with contextlib.suppress(Exception):
            w.close()


def test_following_an_already_closed_capture_ends_at_once(tmp_path):
    """Opened AFTER close(): the length is real from the first read.

    The other tests open the reader while the capture is still growing, so
    the bounded path -- where the header's length is known and anything past
    the payload is the extended header rather than samples -- was never
    taken.
    """
    p = tmp_path / "closed.blue"
    w = _capture(p, n=4096)
    w.close()

    r = Reader(p)
    r.follow_grace_ms = _GRACE_MS
    got = _in_thread(lambda: r.read_follow(4096))
    assert len(got) == 4096

    tail = _in_thread(lambda: r.read_follow(4096))
    assert len(tail) == 0
    assert r.ending == "eof"
