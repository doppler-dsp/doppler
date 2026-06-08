"""fft_demo.py — smoke test for FFT examples from docs/examples/python-fft.md."""

from doppler.spectral import FFT, FFT2D
import numpy as np

rng = np.random.default_rng(0)

# 1-D CF32
x32 = (rng.standard_normal(1024) + 1j * rng.standard_normal(1024)).astype(
    np.complex64
)
f = FFT(1024)
X32 = f.execute_cf32(x32)
assert X32.dtype == np.complex64, f"expected complex64, got {X32.dtype}"
assert X32.shape == (1024,)

# Parseval: Σ|x|² == Σ|X|² / N  (within CF32 tolerance)
err = abs(np.sum(np.abs(x32) ** 2) - np.sum(np.abs(X32) ** 2) / 1024)
assert err < 1.0, f"Parseval error too large: {err:.3e}"

# 1-D CF64
x64 = rng.standard_normal(1024) + 1j * rng.standard_normal(1024)
X64 = f.execute_cf64(x64)
assert X64.dtype == np.complex128, f"expected complex128, got {X64.dtype}"
err64 = abs(np.sum(np.abs(x64) ** 2) - np.sum(np.abs(X64) ** 2) / 1024)
assert err64 < 1e-6, f"CF64 Parseval error too large: {err64:.3e}"

# In-place variant returns the same spectrum as the out-of-place call
X32_ip = f.execute_inplace_cf32(x32.copy())
assert np.allclose(np.abs(X32_ip), np.abs(X32), atol=1e-4)

# Reuse the plan
for _ in range(10):
    out = f.execute_cf32(x32)
assert np.allclose(out, X32, atol=1e-5)

# 2-D FFT
x2 = (
    rng.standard_normal((64, 64)) + 1j * rng.standard_normal((64, 64))
).astype(np.complex64)
f2 = FFT2D(64, 64)
out2 = f2.execute_cf32(x2.ravel())
assert out2.dtype == np.complex64
assert len(out2) == 64 * 64

# 2-D Parseval
err2 = abs(np.sum(np.abs(x2) ** 2) - np.sum(np.abs(out2) ** 2) / (64 * 64))
assert err2 < 10.0, f"2D Parseval error: {err2:.3e}"

print("fft_demo: OK")
