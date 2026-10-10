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
