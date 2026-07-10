import numpy as np
import pytest

from doppler.detection import LockDet


def test_create():
    obj = LockDet(1.0, 1.0, 1, 1)
    assert obj is not None


def test_step_runs():
    obj = LockDet(1.0, 1.0, 1, 1)
    y = obj.step(1.0)
    assert isinstance(y, int)


def test_steps_shape_dtype():
    obj = LockDet(1.0, 1.0, 1, 1)
    x = np.ones(64, dtype=np.float64)
    y = obj.steps(x)
    assert y.shape == (64,)
    assert y.dtype == np.int32


def test_steps_out_param():
    x = np.ones(64, dtype=np.float64)
    buf = np.zeros(64, dtype=np.int32)
    obj1 = LockDet(1.0, 1.0, 1, 1)
    ret = obj1.steps(x, buf)
    assert ret is buf
    obj2 = LockDet(1.0, 1.0, 1, 1)
    np.testing.assert_array_equal(ret, obj2.steps(x))


def test_getter_setter():
    d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=3)
    assert (d.up_thresh, d.down_thresh) == (1.5, 1.2)
    assert (d.n_up, d.n_down) == (2, 3)
    assert d.cnt == 0 and d.locked is False
    d.up_thresh = 1.6
    d.down_thresh = 1.1
    assert (d.up_thresh, d.down_thresh) == (1.6, 1.1)


def test_declare_exactly_at_n_up():
    d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=3, n_down=2)
    assert [d.step(2.0), d.step(2.0)] == [0, 0]
    assert d.cnt == 2
    assert d.step(2.0) == 1
    assert d.cnt == 0 and d.locked is True


def test_single_miss_resets_declare_run():
    d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=3, n_down=2)
    d.step(2.0)
    d.step(2.0)
    assert d.step(1.3) == 0  # band counts as a miss while unlocked
    assert d.cnt == 0
    assert [d.step(2.0), d.step(2.0), d.step(2.0)] == [0, 0, 1]


def test_hysteresis_band_is_sticky_while_locked():
    d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=1, n_down=2)
    assert d.step(2.0) == 1
    assert d.step(1.3) == 1  # in the band: no drop progress
    assert d.cnt == 0
    assert d.step(1.0) == 1  # miss 1 of 2
    assert d.step(1.3) == 1  # a hit resets the drop run
    assert d.cnt == 0
    assert [d.step(1.0), d.step(1.0)] == [1, 0]


def test_configure_preserves_lock_restarts_run():
    d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=1, n_down=3)
    d.step(2.0)
    d.step(1.0)  # drop miss 1 of 3 in flight
    assert d.cnt == 1
    d.configure(2.0, 1.6, 4, 4)
    assert d.locked is True and d.cnt == 0
    assert (d.up_thresh, d.n_up, d.n_down) == (2.0, 4, 4)


def test_steps_matches_step_sequence():
    seq = np.array([2.0, 2.0, 1.3, 1.0, 1.0, 2.0, 2.0, 1.0])
    a = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=2)
    b = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=2)
    np.testing.assert_array_equal(
        a.steps(seq), np.array([b.step(x) for x in seq])
    )


def test_reset():
    d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=1, n_down=1)
    d.step(2.0)
    assert d.locked is True
    d.reset()
    assert d.locked is False and d.cnt == 0
    assert d.up_thresh == 1.5  # config survives


def test_context_manager():
    with LockDet(1.0, 1.0, 1, 1) as obj:
        y = obj.step(1.0)
    assert isinstance(y, int)


def test_destroy():
    obj = LockDet(1.0, 1.0, 1, 1)
    obj.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        obj.step(1.0)
