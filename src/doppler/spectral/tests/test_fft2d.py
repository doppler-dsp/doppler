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


@pytest.mark.parametrize(
    ("ny", "nx"),
    [(6, 10), (5, 7), (12, 20), (3, 9), (8, 6)],
)
@pytest.mark.parametrize("sign", [-1, 1])
def test_non_power_of_two_columns_match_numpy(ny: int, nx: int, sign: int):
    """The column pass has two paths and this takes the one nothing else did.

    `nx` a power of two transposes, so the column FFTs run on contiguous
    rows. Any other `nx` gathers each column into a scratch row, transforms
    it and scatters it back. Every other test in this file uses 4x4 or 64x64,
    so the gather path ran nowhere: a mistake in it (a wrong stride, a column
    left untransformed) would have passed the suite. The reference is
    numpy's own fft2, which is unnormalised forward and, with `ifft2`
    rescaled by ny*nx, unnormalised inverse -- doppler's contract.

    `(8, 6)` is the control: a power-of-two `ny` with a gathered `nx`, so the
    row count cannot be what selects the path.
    """
    rng = np.random.default_rng(1658)
    x = rng.standard_normal(ny * nx) + 1j * rng.standard_normal(ny * nx)
    got = FFT2D(ny, nx, sign, 1).execute_cf64(x)
    grid = x.reshape(ny, nx)
    ref = (
        np.fft.fft2(grid) if sign < 0 else np.fft.ifft2(grid) * (ny * nx)
    ).reshape(-1)
    np.testing.assert_allclose(got, ref, rtol=1e-11, atol=1e-11 * ny * nx)
