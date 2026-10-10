"""The receiver example's dropped-frame count survives a sequence reset.

``src/doppler/examples/receiver.py`` counts the frames missing between
consecutive header ``sequence`` numbers. It subtracted unconditionally, so a
publisher restarting at 0 added a negative to ``dropped`` (#2017). The rule
is ``frames_missing``; the C twin is ``native/examples/receiver_seq.h``,
held by ``native/tests/test_receiver_seq.c``.
"""

from __future__ import annotations

from doppler.examples.receiver import frames_missing


def test_a_forward_jump_counts_the_frames_between() -> None:
    assert frames_missing(4, 5) == 0
    assert frames_missing(4, 7) == 2


def test_a_repeat_or_a_reset_counts_none() -> None:
    assert frames_missing(4, 4) == 0  # a redelivery
    assert frames_missing(1000, 0) == 0  # a restarted publisher


def test_a_stream_with_a_gap_and_a_restart() -> None:
    """The example's loop: one real gap (2 -> 5), then a restart (5 -> 0).
    Two dropped frames; it used to read two minus six."""
    seqs = [0, 1, 2, 5, 0, 1, 2, 3]
    dropped = sum(frames_missing(a, b) for a, b in zip(seqs, seqs[1:]))
    assert dropped == 2
