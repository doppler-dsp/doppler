"""The shared per-frame slip counter, `_frame_slips`, on known slips.

Noiseless BPSK frames of the specification's shape (idle run, dotted
preamble, data from the sync word on), with symbols dropped or repeated
at chosen places in the decided stream, so every count has a known answer
and a slip booked to the wrong region fails by name.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.track.tests._frame_slips import (
    TILE_SPAN,
    frame_markers,
    frame_slips,
    lost_from,
)

GAP, DOTS, BODY, NMK = 367, 128, 2000, 256


def _stream(frames: int, seed: int = 1):
    """Truth, per-frame (sync, end), and the clean decided symbols."""
    rng = np.random.default_rng(seed)
    parts, sync, ends, t = [], [], [], 0
    for _ in range(frames):
        parts.append(
            np.r_[np.zeros(GAP), np.arange(DOTS) % 2, rng.integers(0, 2, BODY)]
        )
        sync.append(t + GAP + DOTS)
        t += GAP + DOTS + BODY
        ends.append(t)
    truth = np.concatenate(parts).astype(np.uint8)
    return truth, sync, ends, (1.0 - 2.0 * truth).astype(np.complex64)


def _edit(sym, drops=(), repeats=()):
    """Drop the symbol at each index in `drops`; repeat each in `repeats`."""
    out = []
    for i, s in enumerate(sym):
        if i in drops:
            continue
        out.append(s)
        if i in repeats:
            out.append(s)
    return np.asarray(out, np.complex64)


def _counts(fs):
    return [(f.gap, f.data, f.other) for f in fs]


def test_clean_stream_slips_nowhere():
    truth, sync, ends, sym = _stream(4)
    fs = frame_slips(sym, truth, sync, ends)
    assert _counts(fs) == [(0, 0, 0)] * 4
    assert all(lag == 0 for f in fs for lag in f.lags)


@pytest.mark.parametrize("where", [GAP // 2, GAP + DOTS // 2])
def test_a_slip_in_the_idle_run_or_dots_is_a_gap_slip(where):
    """The idle run and the dots are one region: neither carries a marker."""
    truth, sync, ends, sym = _stream(3)
    rx = _edit(sym, drops={ends[0] + where})
    assert _counts(frame_slips(rx, truth, sync, ends)) == [
        (0, 0, 0),
        (1, 0, 0),
        (0, 0, 0),
    ]


def test_a_slip_in_data_is_a_data_slip_of_its_own_frame():
    truth, sync, ends, sym = _stream(3)
    rx = _edit(sym, repeats={sync[1] + 1000})
    assert _counts(frame_slips(rx, truth, sync, ends)) == [
        (0, 0, 0),
        (0, 1, 0),
        (0, 0, 0),
    ]


def test_a_slip_that_returns_inside_a_bracket_is_not_counted():
    """The stated bound: the count is net per bracket."""
    truth, sync, ends, sym = _stream(2)
    rx = _edit(sym, drops={sync[1] + 300}, repeats={sync[1] + 400})
    assert _counts(frame_slips(rx, truth, sync, ends))[1] == (0, 0, 0)


def test_two_slips_across_one_gap_count_two():
    truth, sync, ends, sym = _stream(2)
    rx = _edit(sym, drops={ends[0] + 10, ends[0] + 200})
    assert _counts(frame_slips(rx, truth, sync, ends))[1] == (2, 0, 0)


def test_markers_tile_each_frame_from_sync_to_end():
    ms = frame_markers([400, 2000], [1000, 2100], n_marker=NMK)
    assert [(m.frame, m.t, m.n, m.head, m.last, m.tiles) for m in ms] == [
        (0, 400, 256, True, False, True),
        (0, 656, 256, False, False, True),
        (0, 744, 256, False, True, False),
        (1, 2000, 100, True, True, False),
    ]


def test_the_docstring_examples_run():
    """The module's examples are worked cases; run them as tests."""
    import doctest

    import doppler.track.tests._frame_slips as mod

    res = doctest.testmod(mod, optionflags=doctest.ELLIPSIS)
    assert res.attempted > 0 and res.failed == 0, res


def test_a_jump_past_the_narrow_span_is_reacquired_and_counted():
    """40 symbols dropped in one idle run: past `lag_span`, still counted.

    Searching only around the old lag, every later marker would miss and
    the run would read as lost from frame 1, or worse, as a small count.
    """
    truth, sync, ends, sym = _stream(4)
    rx = np.delete(sym, np.arange(ends[0] + 50, ends[0] + 90))
    fs = frame_slips(rx, truth, sync, ends)
    assert lost_from(fs) is None
    assert _counts(fs) == [(0, 0, 0), (40, 0, 0), (0, 0, 0), (0, 0, 0)]
    assert all(lag is not None for f in fs for lag in f.lags)


def test_a_stream_that_is_gone_reads_as_lost_not_as_a_count():
    """Noise from frame 2 on: the counter says lost from 2, not zero slips."""
    truth, sync, ends, sym = _stream(5)
    rng = np.random.default_rng(9)
    rx = sym.copy()
    rx[ends[1] :] = (1 - 2.0 * rng.integers(0, 2, rx.size - ends[1])).astype(
        np.complex64
    )
    fs = frame_slips(rx, truth, sync, ends)
    assert lost_from(fs) == 2
    assert [f.lost for f in fs] == [False, False, True, True, True]


def test_the_stream_is_reacquired_after_a_lost_frame():
    """A frame of noise hiding a 40-symbol jump: re-found at the next head.

    Every marker of frame 1 misses, its wide searches too, so the counter
    waits for frame 2's head and searches wide there. The jump spans the
    missed markers, so it is booked to `other`, not lost and not dropped.
    """
    truth, sync, ends, sym = _stream(4)
    rng = np.random.default_rng(9)
    rx = sym.copy()
    n1 = ends[1] - ends[0]
    rx[ends[0] : ends[1]] = (1 - 2.0 * rng.integers(0, 2, n1)).astype(
        np.complex64
    )
    rx = np.delete(rx, np.arange(ends[0] + 50, ends[0] + 90))
    fs = frame_slips(rx, truth, sync, ends)
    assert lost_from(fs) is None
    assert all(lag is None for lag in fs[1].lags)
    assert all(lag is not None for f in fs[2:] for lag in f.lags)
    assert sum(f.other for f in fs) == 40
    assert sum(f.gap + f.data for f in fs) == 0


def test_a_capture_that_stops_early_is_not_lost():
    """Markers past the end of `rx` were never received: not a loss."""
    truth, sync, ends, sym = _stream(3)
    fs = frame_slips(sym[: ends[1] + 600], truth, sync, ends)
    assert lost_from(fs) is None
    assert _counts(fs) == [(0, 0, 0)] * 3


def _noise(n: int, seed: int = 5) -> np.ndarray:
    rng = np.random.default_rng(seed)
    return (1 - 2.0 * rng.integers(0, 2, n)).astype(np.complex64)


@pytest.mark.parametrize("jump", [TILE_SPAN + 77, 3000])
def test_a_jump_past_the_meters_own_span_is_still_refound(jump):
    """`BerMeter.align` narrows any span to `TILE_SPAN`; the counter tiles.

    A jump just past the meter's limit, and one three tiles out, must both
    be re-found and counted, not read as lost.
    """
    truth, sync, ends, sym = _stream(4)
    cut = ends[0] + 50
    rx = np.r_[sym[:cut], _noise(jump), sym[cut:]].astype(np.complex64)
    fs = frame_slips(rx, truth, sync, ends)
    assert lost_from(fs) is None
    assert sum(f.gap + f.data + f.other for f in fs) == jump


def test_a_jump_past_reacquire_span_reads_as_lost():
    """The other side of the limit: further than asked is lost, not small."""
    truth, sync, ends, sym = _stream(5)
    cut = ends[0] + 50
    rx = np.r_[sym[:cut], _noise(5000), sym[cut:]].astype(np.complex64)
    fs = frame_slips(rx, truth, sync, ends, reacquire_span=4096)
    assert lost_from(fs) == 1


def test_a_miss_in_the_last_frame_is_unresolved_not_lost():
    """No later frame head to search: the stream was not shown lost."""
    truth, sync, ends, sym = _stream(3)
    # The capture runs on past the last frame, so the misses are well
    # inside it: only the absence of a later frame head spares them.
    rx = np.r_[sym[: sync[2] + 600], _noise(sym.size - sync[2] + 5400)]
    fs = frame_slips(rx.astype(np.complex64), truth, sync, ends)
    assert lost_from(fs) is None
    assert fs[2].lags[0] == 0 and fs[2].lags[-1] is None
