# Doppler

**Dead-simple, ultra-fast digital signal processing.**

Doppler is a C99 DSP library covering the core building blocks: NCO,
FIR filter, FFT, polyphase resampling, and ZMQ-based signal streaming.
Python and Rust bindings wrap the same C core — no reimplementation,
no divergence between languages, C throughput from Python.

## Language support

| Language | Status |
|----------|--------|
| C | Native — full API |
| Python | FFT, NCO, FIR, DDC, Resampler, Streaming, Buffers, Accumulator, Delay (`doppler-dsp`) |
| Rust | FFI bindings (`ffi/rust/`) — NCO, FFT, FIR, accumulator, SIMD |
| C++ | Works via `extern "C"` headers — no wrapper required |

## Quick start

=== "Python"

    ```python
    from doppler.source import LO
    import numpy as np

    lo = LO(0.25)              # normalised frequency: 0.25 → Fs/4
    iq = lo.steps(8)
    # [ 1.+0.j  0.+1.j -1.+0.j  0.-1.j ... ]
    ```

    ```python
    from doppler.spectral import FFT
    import numpy as np

    x = (np.random.randn(1024) + 1j * np.random.randn(1024)).astype(np.complex64)
    f = FFT(1024)
    X = f.execute(x)           # complex64 in → complex64 out
    ```

=== "C"

    ```c
    #include <doppler.h>
    #include <complex.h>

    lo_state_t *lo = lo_create(0.25);
    float complex out[1024];
    lo_steps(lo, 1024, out);
    lo_destroy(lo);
    ```

    ```c
    #include "fft/fft_core.h"
    #include <complex.h>

    fft_state_t *fft = fft_create(1024, -1, 1);
    double complex in[1024], out[1024];
    fft_execute_cf64(fft, in, N, out);
    fft_destroy(fft);
    ```

## Benchmarks

Example performance on AMD Ryzen 7 AI 350, 24 GB RAM, release build (`-O2`).

!!! tip "Optimized for speed"
    **Tens of MSa/s to 15+ GSa/s depending on the block.**


| Block | MSa/s |
|-------|------:|
| NCO (raw u32) | 15 599 |
| LO CF32 (65 536-entry LUT) | 1 805 |
| FIR CF32 (19 real taps) | 901 |
| FFT CF32 (N=4096) | 181 |
| Polyphase resampler (2× decim) | 72 |

## Build

```bash
make          # build (Linux/macOS; MSYS2 on Windows)
make test     # run CTest suite
make && uv sync  # with Python bindings
```

## Licensing

The doppler source code is MIT-licensed.

The default build uses pocketfft (BSD-3-Clause), keeping the binary
MIT-compatible. To opt into FFTW for higher performance, build with
`-DUSE_FFTW=ON` — the resulting binary links against FFTW (LGPL).
