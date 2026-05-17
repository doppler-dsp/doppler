import numpy as np
import pytest

from doppler.accumulator import AccCf64


def test_create():
    obj = AccCf64(0j)
    assert obj is not None


def test_initial_value():
    obj = AccCf64(3.0 + 4.0j)
    g = obj.get()
    assert abs(g.real - 3.0) < 1e-10
    assert abs(g.imag - 4.0) < 1e-10


def test_step_push():
    obj = AccCf64(0j)
    obj.step(1.0 + 2.0j)
    obj.step(3.0 + 4.0j)
    g = obj.get()
    assert abs(g.real - 4.0) < 1e-10
    assert abs(g.imag - 6.0) < 1e-10


def test_step_returns_none():
    obj = AccCf64(0j)
    assert obj.step(1.0 + 0j) is None


def test_steps_batch_add():
    obj = AccCf64(0j)
    batch = np.array([1 + 0j, 0 + 1j, 1 + 1j], dtype=np.complex128)
    obj.steps(batch)
    g = obj.get()
    assert abs(g.real - 2.0) < 1e-10
    assert abs(g.imag - 2.0) < 1e-10


def test_steps_returns_none():
    obj = AccCf64(0j)
    x = np.ones(4, dtype=np.complex128)
    assert obj.steps(x) is None


def test_reset():
    obj = AccCf64(0j)
    obj.step(5.0 + 6.0j)
    obj.reset()
    g = obj.get()
    assert abs(g.real) < 1e-10
    assert abs(g.imag) < 1e-10


def test_get_non_destructive():
    obj = AccCf64(0j)
    obj.step(1.0 + 2.0j)
    assert obj.get() == obj.get()


def test_dump():
    obj = AccCf64(0j)
    obj.step(5.0 + 6.0j)
    v = obj.dump()
    assert abs(v.real - 5.0) < 1e-10
    assert abs(v.imag - 6.0) < 1e-10
    g = obj.get()
    assert abs(g.real) < 1e-10
    assert abs(g.imag) < 1e-10


def test_madd():
    obj = AccCf64(0j)
    sig = np.array([1 + 1j, 2 + 2j, 3 + 3j], dtype=np.complex128)
    w = np.array([1.0, 0.5, 0.25], dtype=np.float32)
    obj.madd(sig, w)
    # (1+1j)*1.0 + (2+2j)*0.5 + (3+3j)*0.25 = (2.75+2.75j)
    g = obj.get()
    assert abs(g.real - 2.75) < 1e-10
    assert abs(g.imag - 2.75) < 1e-10


def test_madd_mismatched_lengths():
    obj = AccCf64(0j)
    x = np.array([1 + 0j, 2 + 0j, 3 + 0j], dtype=np.complex128)
    h = np.array([1.0, 1.0], dtype=np.float32)
    obj.madd(x, h)
    g = obj.get()
    assert abs(g.real - 3.0) < 1e-10  # min(3,2)=2 elements


def test_add2d():
    obj = AccCf64(0j)
    x = np.array([1 + 1j, 2 + 2j], dtype=np.complex128)
    obj.add2d(x)
    g = obj.get()
    assert abs(g.real - 3.0) < 1e-10
    assert abs(g.imag - 3.0) < 1e-10


def test_madd2d():
    obj = AccCf64(0j)
    x = np.array([2 + 2j, 4 + 4j], dtype=np.complex128)
    h = np.array([0.5, 0.25], dtype=np.float32)
    obj.madd2d(x, h)
    # (2+2j)*0.5 + (4+4j)*0.25 = (2+2j)
    g = obj.get()
    assert abs(g.real - 2.0) < 1e-10
    assert abs(g.imag - 2.0) < 1e-10


def test_accumulation_across_calls():
    obj = AccCf64(0j)
    obj.step(1.0 + 0j)
    obj.steps(np.array([0 + 1j] * 4, dtype=np.complex128))
    g = obj.get()
    assert abs(g.real - 1.0) < 1e-10
    assert abs(g.imag - 4.0) < 1e-10


def test_context_manager():
    with AccCf64(0j) as obj:
        obj.step(1.0 + 1.0j)
        g = obj.get()
        assert abs(g.real - 1.0) < 1e-10


def test_destroy():
    obj = AccCf64(0j)
    obj.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        obj.step(1.0 + 0j)
