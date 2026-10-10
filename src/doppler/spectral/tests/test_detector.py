import numpy as np

from doppler.spectral import CorrDetector


def test_create():
    obj = CorrDetector(
        np.zeros(1, dtype=np.complex64),
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
    with CorrDetector(
        np.zeros(1, dtype=np.complex64),
        dwell=1,
        noise_lo=0,
        noise_hi=0,
        noise_mode="mean",
        threshold=0.0,
        nthreads=1,
    ):
        pass


def test_destroy():
    obj = CorrDetector(
        np.zeros(1, dtype=np.complex64),
        dwell=1,
        noise_lo=0,
        noise_hi=0,
        noise_mode="mean",
        threshold=0.0,
        nthreads=1,
    )
    obj.destroy()


def test_last_corr_none_before_any_hit():
    obj = CorrDetector(
        np.zeros(4, dtype=np.complex64),
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
    obj = CorrDetector(
        ref,
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
    obj = CorrDetector(ref, threshold=0.0)
    assert obj.dwell == 1
    assert obj.push(ref) is not None


def test_overflowed_push_leaves_the_stream_frame_aligned():
    # Python's push() has room for 1024 detections, and at threshold 0
    # every frame fires, so a frame-aligned chunk of 1030 frames fills it.
    # The 6 frames past the room are lost WHOLE: once full, a push takes
    # the rest only when it completes no frame, and 48 samples do, so it
    # takes nothing more and the next push starts on a frame boundary.
    # A probe frame with its impulse at sample 5 must then report lag 5.
    # Were the next frame's head carried (the pre-#2018 feed), the probe's
    # first sample would complete that carried frame and the push would
    # report its lag 0 instead.
    n = 8
    ref = np.zeros(n, dtype=np.complex64)
    ref[0] = 1
    obj = CorrDetector(ref, threshold=0.0)
    frames = np.zeros((1030, n), dtype=np.complex64)
    frames[:, 0] = 1
    assert len(obj.push(frames.ravel())) == 1024
    probe = np.zeros(n, dtype=np.complex64)
    probe[5] = 1
    assert [lag for lag, *_ in obj.push(probe)] == [5]


def test_last_corr_none_after_set_state():
    # The correlation vector is not part of the serialized state, so a
    # detector restored from a blob has no last vector to view -- not a
    # view of a buffer it never filled.
    ref = np.ones(4, dtype=np.complex64)
    a = CorrDetector(ref, threshold=0.0)
    a.push(ref)
    assert a.last_corr is not None
    b = CorrDetector(ref, threshold=0.0)
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
    obj = CorrDetector(ref, threshold=0.0)
    stream = np.zeros(2048 * n, dtype=np.complex64)
    stream[::n] = 1
    assert len(obj.push(stream[:8191])) == 1023
    assert len(obj.push(stream[8191:16382])) == 1024
    probe = np.zeros(n, dtype=np.complex64)
    probe[5] = 1
    hits = obj.push(np.concatenate([stream[16382:], probe]))
    assert [lag for lag, *_ in hits] == [0, 5]
