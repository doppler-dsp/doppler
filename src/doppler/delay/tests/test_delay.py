import numpy as np
from doppler.delay import DelayCf64


def test_create():
    obj = DelayCf64(4)
    assert obj is not None
    assert obj.num_taps == 4
    assert obj.capacity == 4


def test_capacity_power_of_two():
    assert DelayCf64(1).capacity == 1
    assert DelayCf64(3).capacity == 4
    assert DelayCf64(5).capacity == 8
    assert DelayCf64(8).capacity == 8


def test_push_ptr_round_trip():
    """After pushing A B C the window is [C, B, A] (newest first)."""
    obj = DelayCf64(3)
    obj.push(1 + 0j)
    obj.push(2 + 0j)
    obj.push(3 + 0j)
    win = obj.ptr()
    assert win.dtype == np.complex128
    assert len(win) == 3
    np.testing.assert_array_almost_equal(win, [3 + 0j, 2 + 0j, 1 + 0j])


def test_push_ptr_combined():
    """push_ptr pushes then returns the updated window."""
    obj = DelayCf64(2)
    obj.push(10 + 0j)
    win = obj.push_ptr(20 + 0j)
    assert len(win) == 2
    np.testing.assert_array_almost_equal(win, [20 + 0j, 10 + 0j])


def test_continuity_across_blocks():
    """Wrap-around in the ring buffer must not corrupt the window."""
    obj = DelayCf64(4)
    for i in range(1, 9):
        obj.push(complex(i))
    win = obj.ptr()
    np.testing.assert_array_almost_equal(win, [8, 7, 6, 5])


def test_write_scalar():
    """write() inserts one sample (same as push at the scalar API level)."""
    obj = DelayCf64(3)
    obj.write(1 + 0j)
    obj.write(2 + 0j)
    obj.write(3 + 0j)
    win = obj.ptr()
    np.testing.assert_array_almost_equal(win, [3 + 0j, 2 + 0j, 1 + 0j])


def test_reset_clears_buffer():
    obj = DelayCf64(4)
    obj.push(1 + 1j)
    obj.push(2 + 2j)
    obj.reset()
    win = obj.ptr()
    np.testing.assert_array_equal(win, np.zeros(4, dtype=np.complex128))


def test_complex_values():
    obj = DelayCf64(2)
    obj.push(1.5 + 2.5j)
    obj.push(-3.0 + 4.0j)
    win = obj.ptr()
    np.testing.assert_array_almost_equal(win, [-3.0 + 4.0j, 1.5 + 2.5j])


def test_ptr_view_semantics():
    """ptr() returns a view into the internal buffer (jm shared-buf pattern).
    Successive calls produce the correct window contents."""
    obj = DelayCf64(2)
    obj.push(1 + 0j)
    obj.push(2 + 0j)
    win1 = obj.ptr().copy()  # take a copy before mutating
    obj.push(3 + 0j)
    win2 = obj.ptr()
    np.testing.assert_array_almost_equal(win1, [2 + 0j, 1 + 0j])
    np.testing.assert_array_almost_equal(win2, [3 + 0j, 2 + 0j])


def test_context_manager():
    with DelayCf64(4) as obj:
        obj.push(1 + 0j)
        win = obj.ptr()
        assert len(win) == 4


def test_destroy():
    obj = DelayCf64(4)
    obj.push(1 + 0j)
    obj.destroy()
