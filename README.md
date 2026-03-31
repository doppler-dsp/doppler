# doppler

**Dead-simple, ultra-fast, digital signal processing.**

[![CI](https://github.com/doppler-dsp/doppler/actions/workflows/ci.yml/badge.svg)](https://github.com/doppler-dsp/doppler/actions/workflows/ci.yml)
[![Docs](https://img.shields.io/badge/docs-doppler--dsp.github.io-blue)](https://doppler-dsp.github.io/doppler/)
[![PyPI](https://img.shields.io/pypi/v/doppler-dsp)](https://pypi.org/project/doppler-dsp/)
[![Python](https://img.shields.io/badge/python-3.12%20|%203.13-blue)](https://pypi.org/project/doppler-dsp/)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![C99](https://img.shields.io/badge/C-C99-blue)](https://en.wikipedia.org/wiki/C99)
[![Rust](https://img.shields.io/badge/Rust-FFI-CE4A00?logo=rust&logoColor=white)](ffi/rust)
[![uv](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/astral-sh/uv/main/assets/badge/v0.json)](https://github.com/astral-sh/uv)
[![Ruff](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/astral-sh/ruff/main/assets/badge/v2.json)](https://github.com/astral-sh/ruff)

doppler is a lean C99 signal processing library built for one goal: maximum throughput with minimum friction — from any language. The full DSP stack lives in one portable core with paper-thin Python bindings and a Rust FFI. No runtime surprises, no framework lock-in.

## What's inside

- **NCO** — 32-bit phase accumulator, 2¹⁶-entry LUT, AVX-512 batch generation, FM ctrl port
- **FIR filter** — AVX-512 complex taps, CI8/CI16/CI32/CF32 input types
- **FFT** — 1D and 2D, selectable backend (FFTW or pocketfft)
- **Polyphase resampler** — continuously-variable rate, Kaiser-designed bank; DPMFS variant (608 B, L1-resident) for cache-sensitive pipelines
- **Halfband decimator** — dedicated 2:1 decimator exploiting halfband symmetry; 375 MSa/s at 60 dB attenuation
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

### Halfband decimator (`dp_hbdecim_cf32`)

`block=65 536 samples × 400 iterations` — `./build/c/bench_hbdecim_c`

| N taps | Attenuation | MSa/s (input) |
|-------:|------------:|--------------:|
| 10 | 40 dB | 540 |
| 19 | 60 dB | 375 |
| 37 | 80 dB | 201 |
| 73 | 80 dB | 105 |

The halfband structure halves the FIR multiply count by exploiting
coefficient symmetry; the pure-delay branch costs one multiply instead of N.

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

## Quick example

**C:**

```c
#include <doppler.h>
#include <dp/fft.h>
#include <complex.h>

size_t shape[] = {1024};
dp_fft_global_setup(shape, 1, -1, 1, "estimate", NULL);

double complex in[1024], out[1024];
/* ... fill in[] ... */
dp_fft1d_execute(in, out);

```

**Python:**

```python
from doppler.fft import fft
import numpy as np

x = np.random.randn(1024) + 1j * np.random.randn(1024)
spectrum = fft(x)
```

## Documentation

**Full docs at [doppler-dsp.github.io/doppler](https://doppler-dsp.github.io/doppler/)**

| Document | Contents |
| -------- | -------- |
| [Quick Start](docs/quickstart.md) | Build, install, run the examples (Docker quickstart included) |
| [Build Guide](docs/build.md) | CMake options, platform notes, Python setup, Docker details |
| [Overview](docs/overview.md) | Architecture and full API reference |
| [Examples](docs/examples.md) | C and Python code examples - FFT, SIMD, streaming |

| [CLAUDE.md](CLAUDE.md) | Development notes and project context (for contributors) |

## Build

```bash
make          # build (Linux/macOS; MSYS2 on Windows)
make test     # run CTest suite
```

Or with Python bindings:

```bash
make && uv sync
```

See [Build Guide](docs/build.md) for platform-specific instructions and all CMake
options.

## Licensing

The doppler source code is MIT-licensed.

If built with FFTW support (default), the resulting binary links against
FFTW, which is licensed under the GNU General Public License (GPL). In this
case, the distributed binary is covered by the GPL.

If built with `-DUSE_FFTW=OFF`, the pocketfft backend is used instead.
pocketfft is BSD-3-Clause-licensed (see `POCKETFFT_LICENSE`) which is compatible with the MIT-license and so the resulting binary remains MIT-licensed with BSD-3-Clause licensed FFT features.

See [Build Guide](docs/build.md) for details and the installed LICENSE files.
