import numpy as np
import pytest

from doppler.spectral import FFT2D


def test_create():
    obj = FFT2D(64, 64, -1, 1)
    assert obj is not None


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with FFT2D(64, 64, -1, 1):
        pass


def test_destroy():
    obj = FFT2D(64, 64, -1, 1)
    obj.destroy()


def test_execute_cf32_out_writes_into_callers_buffer():
    ny, nx = 4, 4
    obj = FFT2D(ny, nx, -1, 1)
    x = np.zeros(ny * nx, dtype=np.complex64)
    x[0] = 1.0
    out = np.zeros(
        max(obj.execute_cf32_max_out(), ny * nx), dtype=np.complex64
    )
    y = obj.execute_cf32(x, out=out)
    assert np.shares_memory(y, out)


def test_execute_cf32_out_undersized_raises():
    ny, nx = 4, 4
    obj = FFT2D(ny, nx, -1, 1)
    with pytest.raises(ValueError):
        obj.execute_cf32(
            np.zeros(ny * nx, dtype=np.complex64),
            out=np.zeros(1, dtype=np.complex64),
        )


def test_execute_cf64_out_writes_into_callers_buffer():
    ny, nx = 4, 4
    obj = FFT2D(ny, nx, -1, 1)
    x = np.zeros(ny * nx, dtype=np.complex128)
    x[0] = 1.0
    out = np.zeros(
        max(obj.execute_cf64_max_out(), ny * nx), dtype=np.complex128
    )
    y = obj.execute_cf64(x, out=out)
    assert np.shares_memory(y, out)


def test_execute_inplace_cf32_out_writes_into_callers_buffer():
    ny, nx = 4, 4
    obj = FFT2D(ny, nx, -1, 1)
    x = np.zeros(ny * nx, dtype=np.complex64)
    x[0] = 1.0
    out = np.zeros(
        max(obj.execute_inplace_cf32_max_out(), ny * nx), dtype=np.complex64
    )
    y = obj.execute_inplace_cf32(x, out=out)
    assert np.shares_memory(y, out)


def test_execute_inplace_cf64_out_writes_into_callers_buffer():
    ny, nx = 4, 4
    obj = FFT2D(ny, nx, -1, 1)
    x = np.zeros(ny * nx, dtype=np.complex128)
    x[0] = 1.0
    out = np.zeros(
        max(obj.execute_inplace_cf64_max_out(), ny * nx), dtype=np.complex128
    )
    y = obj.execute_inplace_cf64(x, out=out)
    assert np.shares_memory(y, out)
