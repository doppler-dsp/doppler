"""
Tests for doppler.fft — covers 1D, 2D, inplace, round-trip, dispatcher, and one-shot.
"""

import numpy as np
import pytest

import doppler.fft as nfft


# ---------------------------------------------------------------------------
# 1-D FFT
# ---------------------------------------------------------------------------


class TestFFT1D:
    def test_impulse(self):
        """FFT of a unit impulse is a flat spectrum (all ones)."""
        n = 16
        x = np.zeros(n, dtype=np.complex128)
        x[0] = 1.0

        nfft.setup((n,), sign=-1)
        X = nfft.execute1d(x)

        assert X.shape == (n,)
        assert X.dtype == np.complex128
        assert np.allclose(X, np.ones(n, dtype=np.complex128), atol=1e-12)

    def test_dc(self):
        """FFT of a constant signal has all energy at bin 0."""
        n = 32
        x = np.full(n, 3.0 + 0j, dtype=np.complex128)

        nfft.setup((n,), sign=-1)
        X = nfft.execute1d(x)

        assert abs(X[0] - 3.0 * n) < 1e-10
        assert np.allclose(X[1:], 0, atol=1e-10)

    def test_cosine_tone(self):
        """FFT of cos(2*pi*k/N) puts energy at bins k and N-k."""
        n = 64
        k = 5
        t = np.arange(n, dtype=np.float64)
        x = np.cos(2 * np.pi * k * t / n).astype(np.complex128)

        nfft.setup((n,), sign=-1)
        X = nfft.execute1d(x)

        mag = np.abs(X)
        assert mag[k] > n / 2 - 1  # ~N/2 at bin k
        assert mag[n - k] > n / 2 - 1  # ~N/2 at bin N-k

        # All other bins should be near zero
        mask = np.ones(n, dtype=bool)
        mask[k] = False
        mask[n - k] = False
        assert np.allclose(X[mask], 0, atol=1e-8)

    def test_round_trip(self):
        """Forward then inverse FFT recovers the original signal."""
        n = 128
        rng = np.random.default_rng(42)
        x = rng.standard_normal(n) + 1j * rng.standard_normal(n)
        x = x.astype(np.complex128)

        # Forward
        nfft.setup((n,), sign=-1)
        X = nfft.execute1d(x)

        # Inverse
        nfft.setup((n,), sign=1)
        x_rec = nfft.execute1d(X)

        # IFFT requires dividing by N
        x_rec /= n
        assert np.allclose(x, x_rec, atol=1e-10)

    def test_matches_numpy(self):
        """Our FFT matches numpy.fft.fft for random input."""
        n = 256
        rng = np.random.default_rng(123)
        x = (rng.standard_normal(n) + 1j * rng.standard_normal(n)).astype(np.complex128)

        nfft.setup((n,), sign=-1)
        X_dp = nfft.execute1d(x)
        X_np = np.fft.fft(x)

        assert np.allclose(X_dp, X_np, atol=1e-10)


# ---------------------------------------------------------------------------
# 1-D in-place FFT
# ---------------------------------------------------------------------------


class TestFFT1DInplace:
    def test_inplace_matches_numpy(self):
        """In-place FFT matches numpy.fft.fft."""
        n = 64
        rng = np.random.default_rng(7)
        x = (rng.standard_normal(n) + 1j * rng.standard_normal(n)).astype(np.complex128)

        X_np = np.fft.fft(x)

        x_ip = x.copy()
        nfft.setup((n,), sign=-1)
        result = nfft.execute1d_inplace(x_ip)

        assert result is x_ip  # should return the same buffer
        assert np.allclose(x_ip, X_np, atol=1e-10)

    def test_inplace_modifies_array(self):
        """In-place FFT actually modifies the input array."""
        n = 16
        x = np.zeros(n, dtype=np.complex128)
        x[0] = 1.0
        x_orig = x.copy()

        nfft.setup((n,), sign=-1)
        nfft.execute1d_inplace(x)

        # After FFT of impulse, all bins == 1 (not the original impulse)
        assert not np.array_equal(x, x_orig)
        assert np.allclose(x, 1.0, atol=1e-12)


# ---------------------------------------------------------------------------
# 2-D FFT
# ---------------------------------------------------------------------------


class TestFFT2D:
    def test_impulse_2d(self):
        """FFT of 2D impulse is flat spectrum."""
        ny, nx = 8, 8
        x = np.zeros((ny, nx), dtype=np.complex128)
        x[0, 0] = 1.0

        nfft.setup((ny, nx), sign=-1)
        X = nfft.execute2d(x)

        assert X.shape == (ny, nx)
        assert np.allclose(X, 1.0, atol=1e-12)

    def test_2d_round_trip(self):
        """Forward + inverse 2D FFT recovers input."""
        ny, nx = 16, 16
        rng = np.random.default_rng(99)
        x = (rng.standard_normal((ny, nx)) + 1j * rng.standard_normal((ny, nx))).astype(
            np.complex128
        )

        nfft.setup((ny, nx), sign=-1)
        X = nfft.execute2d(x)

        nfft.setup((ny, nx), sign=1)
        x_rec = nfft.execute2d(X)
        x_rec /= ny * nx

        assert np.allclose(x, x_rec, atol=1e-10)

    def test_2d_matches_numpy(self):
        """Our 2D FFT matches numpy.fft.fft2."""
        ny, nx = 16, 32
        rng = np.random.default_rng(55)
        x = (rng.standard_normal((ny, nx)) + 1j * rng.standard_normal((ny, nx))).astype(
            np.complex128
        )

        nfft.setup((ny, nx), sign=-1)
        X_dp = nfft.execute2d(x)
        X_np = np.fft.fft2(x)

        assert np.allclose(X_dp, X_np, atol=1e-10)


# ---------------------------------------------------------------------------
# 2-D in-place FFT
# ---------------------------------------------------------------------------


class TestFFT2DInplace:
    def test_inplace_matches_numpy(self):
        """2D in-place FFT matches numpy.fft.fft2."""
        ny, nx = 8, 8
        rng = np.random.default_rng(33)
        x = (rng.standard_normal((ny, nx)) + 1j * rng.standard_normal((ny, nx))).astype(
            np.complex128
        )

        X_np = np.fft.fft2(x)

        x_ip = x.copy()
        nfft.setup((ny, nx), sign=-1)
        result = nfft.execute2d_inplace(x_ip)

        assert result is x_ip
        assert np.allclose(x_ip, X_np, atol=1e-10)


# ---------------------------------------------------------------------------
# Dispatcher (execute / execute_inplace)
# ---------------------------------------------------------------------------


class TestDispatcher:
    def test_execute_1d(self):
        """execute() dispatches correctly for 1D input."""
        n = 32
        x = np.zeros(n, dtype=np.complex128)
        x[0] = 1.0

        nfft.setup((n,), sign=-1)
        X = nfft.execute(x)

        assert X.shape == (n,)
        assert np.allclose(X, 1.0, atol=1e-12)

    def test_execute_2d(self):
        """execute() dispatches correctly for 2D input."""
        ny, nx = 4, 4
        x = np.zeros((ny, nx), dtype=np.complex128)
        x[0, 0] = 1.0

        nfft.setup((ny, nx), sign=-1)
        X = nfft.execute(x)

        assert X.shape == (ny, nx)
        assert np.allclose(X, 1.0, atol=1e-12)

    def test_execute_3d_raises(self):
        """execute() raises ValueError for 3D input."""
        x = np.zeros((2, 2, 2), dtype=np.complex128)
        with pytest.raises(ValueError, match="1D and 2D"):
            nfft.execute(x)

    def test_execute_inplace_1d(self):
        """execute_inplace() dispatches correctly for 1D."""
        n = 16
        x = np.zeros(n, dtype=np.complex128)
        x[0] = 1.0

        nfft.setup((n,), sign=-1)
        result = nfft.execute_inplace(x)

        assert result is x
        assert np.allclose(x, 1.0, atol=1e-12)

    def test_execute_inplace_2d(self):
        """execute_inplace() dispatches correctly for 2D."""
        ny, nx = 4, 4
        x = np.zeros((ny, nx), dtype=np.complex128)
        x[0, 0] = 1.0

        nfft.setup((ny, nx), sign=-1)
        result = nfft.execute_inplace(x)

        assert result is x
        assert np.allclose(x, 1.0, atol=1e-12)

    def test_execute_inplace_3d_raises(self):
        """execute_inplace() raises ValueError for 3D input."""
        x = np.zeros((2, 2, 2), dtype=np.complex128)
        with pytest.raises(ValueError, match="1D and 2D"):
            nfft.execute_inplace(x)


# ---------------------------------------------------------------------------
# One-shot fft() convenience function
# ---------------------------------------------------------------------------


class TestOneShot:
    def test_fft_1d(self):
        """fft() one-shot matches manual setup + execute for 1D."""
        n = 64
        rng = np.random.default_rng(11)
        x = (rng.standard_normal(n) + 1j * rng.standard_normal(n)).astype(np.complex128)

        X = nfft.fft(x)
        X_np = np.fft.fft(x)
        assert np.allclose(X, X_np, atol=1e-10)

    def test_fft_2d(self):
        """fft() one-shot works for 2D input."""
        ny, nx = 8, 8
        rng = np.random.default_rng(22)
        x = (rng.standard_normal((ny, nx)) + 1j * rng.standard_normal((ny, nx))).astype(
            np.complex128
        )

        X = nfft.fft(x)
        X_np = np.fft.fft2(x)
        assert np.allclose(X, X_np, atol=1e-10)

    def test_fft_inverse(self):
        """fft() with sign=+1 performs inverse FFT."""
        n = 32
        rng = np.random.default_rng(44)
        x = (rng.standard_normal(n) + 1j * rng.standard_normal(n)).astype(np.complex128)

        X = nfft.fft(x, sign=-1)
        x_rec = nfft.fft(X, sign=1) / n

        assert np.allclose(x, x_rec, atol=1e-10)
