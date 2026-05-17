import numpy as np
import pytest

from doppler.accumulator import AccF32


def test_create():
    obj = AccF32(0.0)
    assert obj is not None


def test_initial_value():
    obj = AccF32(5.0)
    assert obj.get() == pytest.approx(5.0)


def test_step_push():
    obj = AccF32(0.0)
    obj.step(np.float32(1.0))
    obj.step(np.float32(2.0))
    obj.step(np.float32(3.0))
    assert obj.get() == pytest.approx(6.0)


def test_step_returns_none():
    obj = AccF32(0.0)
    assert obj.step(np.float32(1.0)) is None


def test_steps_batch_add():
    obj = AccF32(0.0)
    obj.steps(np.ones(100, dtype=np.float32))
    assert obj.get() == pytest.approx(100.0)


def test_steps_returns_none():
    obj = AccF32(0.0)
    assert obj.steps(np.ones(4, dtype=np.float32)) is None


def test_reset():
    obj = AccF32(0.0)
    obj.steps(np.ones(10, dtype=np.float32))
    obj.reset()
    assert obj.get() == pytest.approx(0.0)


def test_get():
    obj = AccF32(0.0)
    obj.step(np.float32(7.5))
    assert obj.get() == pytest.approx(7.5)
    assert obj.get() == pytest.approx(7.5)  # non-destructive


def test_dump():
    obj = AccF32(0.0)
    obj.step(np.float32(42.0))
    v = obj.dump()
    assert v == pytest.approx(42.0)
    assert obj.get() == pytest.approx(0.0)  # zeroed after dump


def test_madd():
    obj = AccF32(0.0)
    x = np.array([1.0, 2.0, 3.0, 4.0], dtype=np.float32)
    h = np.array([0.25, 0.25, 0.25, 0.25], dtype=np.float32)
    obj.madd(x, h)
    assert obj.get() == pytest.approx(2.5)


def test_madd_mismatched_lengths():
    obj = AccF32(0.0)
    x = np.array([1.0, 2.0, 3.0], dtype=np.float32)
    h = np.array([1.0, 1.0], dtype=np.float32)
    obj.madd(x, h)
    assert obj.get() == pytest.approx(3.0)  # min(3,2)=2 elements


def test_add2d():
    obj = AccF32(0.0)
    obj.add2d(np.arange(10, dtype=np.float32))
    assert obj.get() == pytest.approx(45.0)


def test_madd2d():
    obj = AccF32(0.0)
    x = np.array([2.0, 4.0], dtype=np.float32)
    h = np.array([0.5, 0.25], dtype=np.float32)
    obj.madd2d(x, h)
    assert obj.get() == pytest.approx(2.0)


def test_accumulation_across_calls():
    obj = AccF32(0.0)
    obj.step(np.float32(1.0))
    obj.steps(np.ones(9, dtype=np.float32))
    assert obj.get() == pytest.approx(10.0)
    obj.madd(
        np.array([2.0], dtype=np.float32),
        np.array([5.0], dtype=np.float32),
    )
    assert obj.get() == pytest.approx(20.0)


def test_getter_setter():
    obj = AccF32(0.0)
    assert obj.get_acc() == pytest.approx(0.0)
    obj.set_acc(2.0)
    assert obj.get_acc() == pytest.approx(2.0)


def test_context_manager():
    with AccF32(0.0) as obj:
        obj.step(np.float32(1.0))
        assert obj.get() == pytest.approx(1.0)


def test_destroy():
    obj = AccF32(0.0)
    obj.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        obj.step(np.float32(1.0))
