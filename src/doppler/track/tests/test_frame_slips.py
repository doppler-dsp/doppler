"""The shared per-frame slip counter, `_frame_slips`, on known slips.

Noiseless BPSK frames of the specification's shape (idle run, dotted
preamble, data from the sync word on), with symbols dropped or repeated
at chosen places in the decided stream, so every count has a known answer
and a slip booked to the wrong region fails by name.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.track.tests._frame_slips import frame_markers, frame_slips

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
