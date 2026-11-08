import numpy as np
import pytest

from doppler.correlation import Correlator


def test_create():
    obj = Correlator(
        np.zeros(1, dtype=np.complex64),
        detect=1,
        dwell=1,
        noise_lo=0,
        noise_hi=0,
        noise_mode="mean",
        threshold=0.0,
        nthreads=1,
    )
    assert obj is not None


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with Correlator(
        np.zeros(1, dtype=np.complex64),
        detect=1,
        dwell=1,
        noise_lo=0,
        noise_hi=0,
        noise_mode="mean",
        threshold=0.0,
        nthreads=1,
    ):
        pass


def test_destroy():
    obj = Correlator(
        np.zeros(1, dtype=np.complex64),
        detect=1,
        dwell=1,
        noise_lo=0,
        noise_hi=0,
        noise_mode="mean",
        threshold=0.0,
        nthreads=1,
    )
    obj.destroy()


def test_last_corr_none_before_any_hit():
    obj = Correlator(
        np.zeros(4, dtype=np.complex64),
        detect=1,
        dwell=1,
        noise_lo=0,
        noise_hi=3,
        noise_mode="mean",
        threshold=0.0,
        nthreads=1,
    )
    assert obj.last_corr is None


def test_last_corr_aliases_across_pushes():
    # Documented contract: last_corr is a zero-copy view reused by every
    # push() (threshold=0.0 always fires), not an independent array. A
    # later push() with different data overwrites an earlier-returned view
    # in place, visible through the same handle.
    ref = np.ones(4, dtype=np.complex64)
    obj = Correlator(
        ref,
        detect=1,
        dwell=1,
        noise_lo=0,
        noise_hi=3,
        noise_mode="mean",
        threshold=0.0,
        nthreads=1,
    )
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
    ref = np.ones(4, dtype=np.complex64)
    obj = Correlator(ref, detect=1, threshold=0.0)
    assert obj.dwell == 1
    assert obj.push(ref) is not None


def test_a_long_push_reports_every_frame_and_stays_frame_aligned():
    # push() is sized from push_max_out (docs/design/correlator-python.md
    # §5): no fixed room, so a frame-aligned chunk of 1030 frames at
    # threshold 0, where every frame fires, reports all 1030 -- no more than
    # the bound -- and the stream stays on its frame grid: a probe frame with
    # its impulse at sample 5 then reports lag 5. (Before step 4 the list
    # held 1024 and the 6 frames past it were dropped whole, #1992.)
    n = 8
    ref = np.zeros(n, dtype=np.complex64)
    ref[0] = 1
    obj = Correlator(ref, detect=1, threshold=0.0)
    frames = np.zeros((1030, n), dtype=np.complex64)
    frames[:, 0] = 1
    x = frames.ravel()
    bound = obj.push_max_out(x.size)
    hits = obj.push(x)
    assert len(hits) == 1030 <= bound
    probe = np.zeros(n, dtype=np.complex64)
    probe[5] = 1
    assert [int(lag) for lag in obj.push(probe)["col"]] == [5]


def test_last_corr_none_after_set_state():
    # The correlation vector is not part of the serialized state, so a
    # detector restored from a blob has no last vector to view -- not a
    # view of a buffer it never filled.
    ref = np.ones(4, dtype=np.complex64)
    a = Correlator(ref, detect=1, threshold=0.0)
    a.push(ref)
    assert a.last_corr is not None
    b = Correlator(ref, detect=1, threshold=0.0)
    b.set_state(a.get_state())
    assert b.last_corr is None
    a.push(ref)
    b.push(ref)
    np.testing.assert_array_equal(b.last_corr, a.last_corr)


def test_a_full_push_keeps_a_remainder_shorter_than_a_frame():
    # Chunks of 8191 samples at n = 8, every frame firing.  The first push
    # completes 1023 frames and carries 7; the second completes 1024
    # (7 + 8191 = 1024 * 8 + 6) and fills the room on the last, leaving 6
    # samples that complete no frame.  Those are kept as the carry, so
    # nothing is lost: the stream's last 2 samples finish that frame, and a
    # probe frame with its impulse at sample 5 reports lag 5.  Were the 6
    # dropped, the probe would arrive 6 samples early: lag 7, alone.
    n = 8
    ref = np.zeros(n, dtype=np.complex64)
    ref[0] = 1
    obj = Correlator(ref, detect=1, threshold=0.0)
    stream = np.zeros(2048 * n, dtype=np.complex64)
    stream[::n] = 1
    assert len(obj.push(stream[:8191])) == 1023
    assert len(obj.push(stream[8191:16382])) == 1024
    probe = np.zeros(n, dtype=np.complex64)
    probe[5] = 1
    hits = obj.push(np.concatenate([stream[16382:], probe]))
    assert [int(lag) for lag in hits["col"]] == [0, 5]


def test_a_refused_create_is_a_value_error():
    """dwell=0 is refused as it is for the 1-D map (#2641); the detector,
    its 2-D twin and the 2-D map used to report it as MemoryError, having
    no create_error. The message names dwell
    (docs/design/correlator-python.md §17)."""
    det = "Correlator: dwell must be at least 1"
    with pytest.raises(ValueError, match=det):
        Correlator(np.ones(16, np.complex64), detect=1, dwell=0)
    with pytest.raises(ValueError, match=det):
        Correlator(np.ones((4, 4), np.complex64), detect=1, dwell=0)
    with pytest.raises(ValueError, match=det):
        Correlator(np.ones((4, 4), np.complex64), dwell=0)
