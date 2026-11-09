import numpy as np

from doppler.correlation import Correlator

# Every test here builds the 2-D detector: a ref of at least two rows (a
# one-row ref is the 1-D flavor, test_correlator_detector_1d.py's).
_KW = {
    "detect": 1,
    "dwell": 1,
    "noise_lo": 0,
    "noise_hi": 7,
    "noise_mode": "mean",
    "threshold": 0.0,
    "nthreads": 1,
}


def _det(ref: np.ndarray) -> Correlator:
    obj = Correlator(ref, **_KW)
    assert obj.flavor == "detector_2d"
    return obj


def test_create():
    assert _det(np.zeros((2, 4), dtype=np.complex64)).ny == 2


def test_context_manager():
    with _det(np.zeros((2, 4), dtype=np.complex64)) as obj:
        assert obj.nx == 4


def test_destroy():
    _det(np.zeros((2, 4), dtype=np.complex64)).destroy()


def test_last_corr_none_before_any_hit():
    assert _det(np.ones((2, 4), dtype=np.complex64)).last_corr is None


def test_last_corr_aliases_across_pushes():
    # Documented contract: last_corr is a zero-copy view reused by every
    # push() (threshold=0.0 always fires), not an independent array. A
    # later push() with different data overwrites an earlier-returned view
    # in place, visible through the same handle.
    ref = np.ones((2, 4), dtype=np.complex64)
    obj = _det(ref)
    obj.push(ref)
    first = obj.last_corr
    assert first is not None
    obj.push(-ref)
    second = obj.last_corr
    assert np.shares_memory(first, second)
    np.testing.assert_array_equal(first, second)


def test_dwell_defaults_to_one():
    # Regression: the binding once initialized an omitted dwell to 0
    # (contradicting the manifest default of 1), producing a detector
    # that never int-dumps -- push() could never emit a result.
    ref = np.ones((2, 2), dtype=np.complex64)
    obj = Correlator(ref, detect=1, threshold=0.0)
    assert obj.flavor == "detector_2d"
    assert obj.dwell == 1
    assert obj.push(ref) is not None


def test_a_long_push_reports_every_frame_and_stays_frame_aligned():
    # As the 1-D detector's: push() is sized from push_max_out
    # (docs/design/correlator-python.md §5), so 1030 frames at threshold 0
    # report all 1030, within the bound, and a probe frame with its impulse
    # at (1, 2) then reports (1, 2): the stream stayed on its frame grid.
    # (Before step 4 the list held 1024 and the rest were dropped whole,
    # #1992.)
    ny, nx = 2, 4
    ref = np.zeros((ny, nx), dtype=np.complex64)
    ref[0, 0] = 1
    obj = Correlator(ref, detect=1, threshold=0.0)
    assert obj.flavor == "detector_2d"
    frames = np.zeros((1030, ny, nx), dtype=np.complex64)
    frames[:, 0, 0] = 1
    x = frames.ravel()
    bound = obj.push_max_out(x.size)
    hits = obj.push(x)
    assert len(hits) == 1030 <= bound
    probe = np.zeros((ny, nx), dtype=np.complex64)
    probe[1, 2] = 1
    h = obj.push(probe.ravel())
    assert [(int(r), int(c)) for r, c in zip(h["row"], h["col"])] == [(1, 2)]


def test_last_corr_none_after_set_state():
    # The correlation surface is not part of the serialized state, so a
    # detector restored from a blob has no last surface to view -- not a
    # view of a buffer it never filled.
    ref = np.ones((2, 2), dtype=np.complex64)
    a = Correlator(ref, detect=1, threshold=0.0)
    assert a.flavor == "detector_2d"
    a.push(ref.ravel())
    assert a.last_corr is not None
    b = Correlator(ref, detect=1, threshold=0.0)
    b.set_state(a.get_state())
    assert b.last_corr is None
    a.push(ref.ravel())
    b.push(ref.ravel())
    np.testing.assert_array_equal(b.last_corr, a.last_corr)


def test_a_full_push_keeps_a_remainder_shorter_than_a_frame():
    # Chunks of 8191 samples at 2 x 4, every frame firing.  The first push
    # completes 1023 frames and carries 7; the second completes 1024
    # (7 + 8191 = 1024 * 8 + 6) and fills the room on the last, leaving 6
    # samples that complete no frame.  Those are kept as the carry, so
    # nothing is lost: the stream's last 2 samples finish that frame, and a
    # probe frame with its impulse at (1, 2) reports (1, 2).  Were the 6
    # dropped, the probe would arrive 6 samples early and split across two
    # frames, and (1, 2) would never be reported.
    ny, nx = 2, 4
    ref = np.zeros((ny, nx), dtype=np.complex64)
    ref[0, 0] = 1
    obj = Correlator(ref, detect=1, threshold=0.0)
    assert obj.flavor == "detector_2d"
    stream = np.zeros(2048 * ny * nx, dtype=np.complex64)
    stream[:: ny * nx] = 1
    assert len(obj.push(stream[:8191])) == 1023
    assert len(obj.push(stream[8191:16382])) == 1024
    probe = np.zeros((ny, nx), dtype=np.complex64)
    probe[1, 2] = 1
    hits = obj.push(np.concatenate([stream[16382:], probe.ravel()]))
    assert [(int(r), int(c)) for r, c in zip(hits["row"], hits["col"])] == [
        (0, 0),
        (1, 2),
    ]
