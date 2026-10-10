"""The receiver example's dropped-frame count survives a sequence reset.

``src/doppler/examples/receiver.py`` counts the frames missing between
consecutive header ``sequence`` numbers. It subtracted unconditionally, so a
publisher restarting at 0 added a negative to ``dropped`` (#2017). Its
``main()`` keeps a ``SequenceCount`` and feeds it each frame, so these tests
drive the state the example runs, not a copy of its loop. The C twin is
``native/examples/receiver_seq.h``, held by
``native/tests/test_receiver_seq.c``.

Each stream is red under a plausible wrong rule: no first-frame anchor (the
late join), a backward step counted as a restart from 0 (the restart to 3),
and a high-water mark that only re-anchors forward (the gap after a
restart).
"""

from __future__ import annotations

import doctest
import sys

import numpy as np
import pytest

import doppler.examples.receiver as receiver
from doppler.examples.receiver import SequenceCount, frames_missing


def _dropped(seqs: list[int]) -> int:
    count = SequenceCount()
    for seq in seqs:
        count.feed(seq)
    return count.dropped


def test_a_forward_jump_counts_the_frames_between() -> None:
    assert frames_missing(4, 5) == 0
    assert frames_missing(4, 7) == 2


def test_a_repeat_or_a_backward_step_counts_none() -> None:
    assert frames_missing(4, 4) == 0
    assert frames_missing(1000, 0) == 0


def test_a_late_join_has_dropped_nothing() -> None:
    """The first frame anchors the count: joining at 1,000,000 is not
    999,999 frames lost."""
    assert _dropped([1_000_000, 1_000_001, 1_000_002]) == 0


def test_a_restart_to_a_nonzero_value_adds_nothing() -> None:
    """1000 -> 3 is the publisher coming back, not three frames lost."""
    assert _dropped([998, 999, 1000, 3, 4]) == 0


def test_a_gap_after_a_restart_still_counts() -> None:
    """9 -> 12 drops two, the restart 12 -> 0 none, and 1 -> 3 one more."""
    assert _dropped([7, 8, 9, 12, 0, 1, 3]) == 3


def test_the_reported_stream_with_a_gap_and_a_restart() -> None:
    """One real gap (2 -> 5), then a restart (5 -> 0): two dropped frames;
    it used to read two minus six."""
    assert _dropped([0, 1, 2, 5, 0, 1, 2, 3]) == 2


def test_the_examples_in_its_docstrings_run() -> None:
    """``frames_missing`` and ``SequenceCount`` carry ``>>>`` examples, and
    nothing else runs an example script's doctests."""
    result = doctest.testmod(receiver)
    assert result.attempted >= 4
    assert result.failed == 0


def _scripted(script: list[int | BaseException]) -> type:
    """A stand-in for ``Subscriber`` that replays *script*.

    Each item is a sequence number (a frame) or an exception to raise from
    ``recv()``. A ``KeyboardInterrupt`` ends the run, as Ctrl+C does under
    the example's ``Interrupt`` guard.
    """

    class _Subscriber:
        def __init__(self, endpoint: str) -> None:
            self.items = iter(script)

        def __enter__(self) -> _Subscriber:
            return self

        def __exit__(self, *exc: object) -> None:
            return None

        def recv(self) -> tuple[np.ndarray, dict[str, int]]:
            item = next(self.items)
            if isinstance(item, BaseException):
                raise item
            samples = np.ones(4, dtype=np.complex64)
            return samples, {"sequence": item, "num_samples": 4}

    return _Subscriber


class _Interrupt:
    def __init__(self, signals: object) -> None:
        pass

    def __enter__(self) -> None:
        return None

    def __exit__(self, *exc: object) -> None:
        return None


def test_an_end_of_stream_is_waited_out(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """A publisher's EOS, then its restart from 0: the receiver keeps going.

    ``Subscriber.recv`` raises ``EOFError`` on an end-of-stream. The example
    caught only ``KeyboardInterrupt``, so a graceful publisher restart killed
    the dashboard, while ``receiver.c`` carried on (#2096). Red if the
    ``except EOFError`` is deleted: the error escapes ``main()``.
    """
    script: list[int | BaseException] = [
        5,
        6,
        EOFError("end of stream: the sender finished"),
        0,
        1,
        KeyboardInterrupt(),
    ]
    _run(script, monkeypatch)
    out = capsys.readouterr().out
    assert "End of stream from the publisher" in out
    last = out.rsplit("\033[2J", 1)[-1]
    assert "Packets:      4" in last
    assert "sequence:     1" in last
    # The restart from 0 is not a drop.
    assert "Dropped:      0" in last


def _run(
    script: list[int | BaseException],
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(receiver, "Subscriber", _scripted(script))
    monkeypatch.setattr(receiver, "Interrupt", _Interrupt)
    monkeypatch.setattr(sys, "argv", ["receiver.py"])
    receiver.main()


def test_a_one_off_receive_failure_is_skipped(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """The broker's slow-consumer signal surfaces as one RuntimeError; the
    lost frames are the next forward gap. The dashboard carries on, as
    receiver.c does, and counts them: 3 -> 6 drops 4 and 5."""
    failed = RuntimeError("receive failed")
    _run([1, 2, 3, failed, 6, failed, 7, KeyboardInterrupt()], monkeypatch)
    out = capsys.readouterr()
    last = out.out.rsplit("\033[2J", 1)[-1]
    assert "Packets:      5" in last
    assert "Dropped:      2" in last
    assert out.err.count("Receive failed") == 2


def test_three_failures_in_a_row_end_the_run(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """With no frame between them, three failures are a dead connection:
    the third is raised rather than spun on."""
    failed = RuntimeError("receive failed")
    with pytest.raises(RuntimeError):
        _run([1, failed, failed, failed, 2], monkeypatch)
