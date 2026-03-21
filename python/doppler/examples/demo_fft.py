import numpy as np
import doppler as na


def banner(title):
    print("\n" + "=" * 60)
    print(title)
    print("=" * 60)


# ------------------------------------------------------------
# 1D FFT (out-of-place)
# ------------------------------------------------------------
banner("1D FFT (out-of-place)")

n = 8
x = np.cos(2 * np.pi * np.arange(n) / n).astype(np.complex128)

print("Input:", x)

na.fft.setup((n,), sign=1, nthreads=4, planner="patient", wisdom=None)
y = na.fft.execute1d(x)

print("FFT:", y)

# ------------------------------------------------------------
# 1D FFT (in-place)
# ------------------------------------------------------------
banner("1D FFT (in-place)")

x = np.sin(2 * np.pi * np.arange(n) / n).astype(np.complex128)
print("Input:", x)

na.fft.setup((n,), sign=1, nthreads=4, planner="patient", wisdom=None)
na.fft.execute1d_inplace(x)

print("FFT (in-place):", x)

# ------------------------------------------------------------
# 2D FFT (out-of-place)
# ------------------------------------------------------------
banner("2D FFT (out-of-place)")

ny, nx = 4, 4
x2 = np.arange(ny * nx, dtype=np.complex128).reshape(ny, nx)

print("Input (2D):")
print(x2)

na.fft.setup((ny, nx), sign=1, nthreads=4, planner="patient", wisdom=None)
y2 = na.fft.execute2d(x2)

print("FFT (2D):")
print(y2)

# ------------------------------------------------------------
# 2D FFT (in-place)
# ------------------------------------------------------------
banner("2D FFT (in-place)")

x2 = np.sin(np.arange(ny * nx)).reshape(ny, nx).astype(np.complex128)
print("Input (2D):")
print(x2)

na.fft.setup((ny, nx), sign=1, nthreads=4, planner="patient", wisdom=None)
na.fft.execute2d_inplace(x2)

print("FFT (2D in-place):")
print(x2)

print("\nAll tests completed successfully.")
