import numpy as np

from doppler.spectral import CorrDetector2D


def test_create():
    obj = CorrDetector2D(
        np.zeros((1, 1), dtype=np.complex64), "mean", 1, 0, 0, 0.0, 1
    )
    assert obj is not None


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with CorrDetector2D(
        np.zeros((1, 1), dtype=np.complex64), "mean", 1, 0, 0, 0.0, 1
    ):
        pass


def test_destroy():
    obj = CorrDetector2D(
        np.zeros((1, 1), dtype=np.complex64), "mean", 1, 0, 0, 0.0, 1
    )
    obj.destroy()


def test_last_corr_none_before_any_hit():
    obj = CorrDetector2D(
        np.ones((1, 4), dtype=np.complex64), "mean", 1, 0, 3, 0.0, 1
    )
    assert obj.last_corr is None


def test_last_corr_aliases_across_pushes():
    # Documented contract: last_corr is a zero-copy view reused by every
    # push() (threshold=0.0 always fires), not an independent array. A
    # later push() with different data overwrites an earlier-returned view
    # in place, visible through the same handle.
    ref = np.ones((1, 4), dtype=np.complex64)
    obj = CorrDetector2D(ref, "mean", 1, 0, 3, 0.0, 1)
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
    obj = CorrDetector2D(ref, threshold=0.0)
    assert obj.dwell == 1
    assert obj.push(ref) is not None
