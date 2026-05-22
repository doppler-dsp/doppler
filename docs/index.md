# Doppler

**Dead-simple, ultra-fast digital signal processing.**

Doppler is a C99 DSP library covering the core building blocks: NCO,
FIR filter, FFT, polyphase resampling, and ZMQ-based signal streaming.
Python and Rust bindings wrap the same C core — no reimplementation,
no divergence between languages, C throughput from Python.

## What's inside

- **NCO** — 32-bit phase accumulator, 2¹⁶-entry LUT, AVX-512 batch generation, FM ctrl port
- **FIR filter** — real or complex CF32 taps; CF32 input/output
- **FFT** — 1D and 2D, selectable backend (pocketfft default, FFTW opt-in)
- **Resampler** — polyphase (4096-phase × 19-tap Kaiser bank, 60 dB); halfband 2:1 decimator
- **DDC** — `Ddc` (complex IQ) and `DDCR` (real ADC, Architecture D2, ~2× cheaper)
- **Signal streaming** — ZMQ transport (PUB/SUB, PUSH/PULL, REQ/REP); C and Python; multi-machine
- **Circular buffers** — double-mapped ring buffers, lock-free SPSC, zero-copy IPC (F32/F64/I16)
- **Spectrum analyzer** — `doppler-specan` command (`doppler-dsp[specan-web]`): real-time FFT display, waterfall, web UI
- **Pipeline CLI** — `doppler compose`: wire blocks into processing chains with a YAML file
- **Multi-language** — clean C ABI; Python bindings and Rust FFI; Dopplerfile for custom blocks

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

Release build (`-O2`). Re-run with `make build` then the binary listed.

### NCO / LO

`block=1 048 576 × 200 iters` — `./build/native/src/nco/bench_nco_core`, `./build/native/src/lo/bench_lo_core`

| Variant | MSa/s | Notes |
|---------|------:|-------|
| NCO `u32` | 15 599 | Raw phase accumulator |
| NCO `u32_scaled` | 8 380 | Phase mapped to [0, nmax) |
| NCO `u32_ovf` | 3 298 | Phase + per-sample carry flag |
| LO `cf32` | 1 805 | IQ phasors via 2¹⁶-entry LUT |
| LO `cf32_ctrl` | 559 | LO + per-sample FM deviation |

### FIR

`taps=19  block=65 536 × 100 iters` — `./build/native/src/fir/bench_fir_core`

| Input / taps | MSa/s |
|-------------|------:|
| CF32 / real | 901 |
| CF32 / complex | 519 |

### FFT (pocketfft backend)

`./build/native/src/fft/bench_fft_core`

| Size | CF32 MSa/s | CF64 MSa/s |
|-----:|-----------:|-----------:|
| 1 024 | 241 | 271 |
| 4 096 | 181 | 159 |
| 16 384 | 159 | 160 |

### Polyphase resampler

`4096-phase × 19-tap  block=65 536 × 200 iters` — `./build/native/src/resamp/bench_resamp_core`

| Rate | MSa/s (input) |
|-----:|--------------:|
| 0.5 (2× decim) | 72 |
| 2.0 (2× interp) | 75 |

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
