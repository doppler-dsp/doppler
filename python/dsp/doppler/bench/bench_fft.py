import sys
import numpy as np
import doppler
import time


def main():
    if len(sys.argv) != 2:
        print("Usage: python bench_fft_py.py N")
        sys.exit(1)

    n = int(sys.argv[1])

    x = np.random.randn(n) + 1j * np.random.randn(n)

    t0 = time.perf_counter()
    doppler.fft1d(x)
    t1 = time.perf_counter()

    print(f"Python/native FFT: {t1 - t0:.6f} seconds")


if __name__ == "__main__":
    main()
