# Doppler

**Dead-simple, ultra-fast, digital signal processing.**

Doppler is a lean C99 signal processing library built for one goal:
maximum throughput with minimum friction — from any language. The full
DSP stack lives in one portable core with paper-thin Python bindings
and a Rust FFI. No runtime surprises, no framework lock-in.

## What's inside

- **NCO** — 32-bit phase accumulator, 2¹⁶-entry LUT, AVX-512 batch generation, FM ctrl port
- **FIR filter** — AVX-512 complex taps, CI8/CI16/CI32/CF32 input types
- **FFT** — 1D and 2D, selectable backend (FFTW or pocketfft)
- **SIMD arithmetic** — SSE2/AVX2 complex multiply via `dp_c16_mul`
- **Signal streaming** — low-latency ZMQ transport (PUB/SUB, PUSH/PULL, REQ/REP)
- **Circular buffers** — double-mapped ring buffers for zero-copy, lock-free IPC (F32/F64/I16)
- **Multi-language** — clean C ABI; Python bindings (NCO, FFT, streaming, buffers) and Rust FFI

## Benchmarks

Measured on **AMD Ryzen AI 7 350**, Release build (`-O3 -march=native`).
Re-run any suite with `make blazing` then the binary listed below.

### NCO (`dp_nco_*`)

`block=1 048 576 samples × 200 iterations` — `./build/c/bench_nco_c`

| Rank | Variant | MSa/s | Notes |
|------|---------|------:|-------|
| 🥇 | `u32` | 20 341 | Raw phase only — store + add, no LUT |
| 🥈 | `u32_ovf` | 3 028 | Raw phase + carry bit (ADD + SETB) |
| 🥉 | `cf32` | 2 094 | Free-running IQ — AVX-512 16-wide gather |
| 4 | `u32_ovf_ctrl` | 1 424 | FM ctrl + raw phase + carry |
| 5 | `u32_ctrl` | 1 095 | FM ctrl + raw phase |
| 6 | `cf32_ctrl` | 525 | FM ctrl + IQ — LUT + ctrl overhead |

### FIR (`dp_fir_execute_*`)

`taps=19  block=409 600 samples × 100 iterations` — `./build/c/bench_fir_c`

| Input type | MSa/s | Notes |
|------------|------:|-------|
| `CI8` | 317 | 8-bit complex integer input |
| `CF32` | 280 | 32-bit complex float input |
| `CI16` | 235 | 16-bit complex integer input |

### FFT (`dp_fft*`)

FFTW backend, `estimate` plan, 1 thread, complex double — `./build/c/bench_fft_c`

**1D (`fft1d_execute` / `fft1d_execute_inplace`)**

| Size | Out-of-place MSa/s | In-place MSa/s |
|-----:|-------------------:|---------------:|
| 1 024 | 925 | 757 |
| 4 096 | 474 | 379 |
| 16 384 | 146 | 139 |

**2D (`fft2d_execute` / `fft2d_execute_inplace`)**

| Size | Out-of-place MSa/s | In-place MSa/s |
|-----:|-------------------:|---------------:|
| 64 × 64 | 722 | 616 |
| 128 × 128 | 252 | 241 |
| 256 × 256 | 33 | 30 |

### Ring buffer (`dp_f32 / dp_f64`)

Lock-free SPSC, 268 M samples, batch=4096 — `./build/c/bench_buffer_c`

| Type | MSa/s | GB/s |
|------|------:|-----:|
| `f32` (8 B/sample) | 5 129 | 38.2 |
| `f64` (16 B/sample) | 2 401 | 35.8 |

## Quick start

=== "Python"

    ```python
    from doppler import Nco
    import numpy as np

    with Nco(0.25) as nco:
        iq = nco.execute_cf32(8)
        # [ 1.+0.j  0.+1.j -1.+0.j  0.-1.j ... ]
    ```

    ```python
    from doppler.fft import fft
    import numpy as np

    x = np.random.randn(1024) + 1j * np.random.randn(1024)
    spectrum = fft(x)
    ```

=== "C"

    ```c
    #include <doppler.h>
    #include <dp/nco.h>

    dp_nco_t *nco = dp_nco_create(0.25f);
    dp_cf32_t out[1024];
    dp_nco_execute_cf32(nco, out, 1024);
    dp_nco_destroy(nco);
    ```

    ```c
    #include <doppler.h>
    #include <dp/fft.h>

    size_t shape[] = {1024};
    dp_fft_global_setup(shape, 1, -1, 1, "estimate", NULL);
    double complex in[1024], out[1024];
    dp_fft1d_execute(in, out);
    ```

## Build

```bash
make          # build (Linux/macOS; MSYS2 on Windows)
make test     # run CTest suite
make && uv sync  # with Python bindings
```

## Licensing

The doppler source code is MIT-licensed.

If built with FFTW support (default), the resulting binary links against
FFTW (GPL). Built with `-DUSE_FFTW=OFF`, the pocketfft backend is used
instead (BSD-3-Clause), keeping the binary MIT-licensed.
