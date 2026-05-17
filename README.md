# doppler

**Dead-simple, ultra-fast digital signal processing.**

[![CI](https://github.com/doppler-dsp/doppler/actions/workflows/ci.yml/badge.svg)](https://github.com/doppler-dsp/doppler/actions/workflows/ci.yml)
[![Docs](https://img.shields.io/badge/docs-doppler--dsp.github.io-blue)](https://doppler-dsp.github.io/doppler/)
[![PyPI](https://img.shields.io/pypi/v/doppler-dsp)](https://pypi.org/project/doppler-dsp/)
[![Python](https://img.shields.io/badge/python-3.12%20|%203.13%20|%203.14-blue)](https://pypi.org/project/doppler-dsp/)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![C99](https://img.shields.io/badge/C-C99-blue)](https://en.wikipedia.org/wiki/C99)
[![Rust](https://img.shields.io/badge/Rust-FFI-CE4A00?logo=rust&logoColor=white)](ffi/rust)
[![uv](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/astral-sh/uv/main/assets/badge/v0.json)](https://github.com/astral-sh/uv)
[![Ruff](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/astral-sh/ruff/main/assets/badge/v2.json)](https://github.com/astral-sh/ruff)

doppler is a lean C99 signal processing library built for one goal: maximum throughput with minimum friction — from any language. The full DSP stack lives in one portable core with paper-thin Python bindings and a Rust FFI. No runtime surprises, no framework lock-in.

## What's inside

- **NCO / LO** — 32-bit phase accumulator, 2¹⁶-entry LUT, AVX-512 batch generation, FM ctrl port
- **FIR filter** — AVX-512 complex taps, CI8/CI16/CI32/CF32 input types
- **FFT** — 1D and 2D, selectable backend (FFTW or pocketfft)
- **Polyphase resampler** — continuously-variable rate, built-in 4096-phase × 19-tap Kaiser bank (60 dB)
- **Halfband decimator** — dedicated 2:1 decimator exploiting halfband symmetry; 375 MSa/s at 60 dB
- **DDC / DDCR** — digital down-converter for complex and real ADC input; Architecture D2 for ~2× savings on real input
- **Accumulator** — F32 and CF64 running accumulators with configurable window
- **Delay** — CF64 sample delay line
- **Signal streaming** — low-latency ZMQ transport (PUB/SUB, PUSH/PULL, REQ/REP)
- **Circular buffers** — double-mapped ring buffers for zero-copy, lock-free IPC (F32/F64/I16)
- **Multi-language** — clean C ABI; Python bindings for all modules and Rust FFI

## Benchmarks

Throughput depends heavily on hardware, compiler, and SIMD availability.
On a Ryzen 7 AI 350 (16 GB, `-O2`), typical figures range from
**hundreds of MSa/s** (FFT, FIR complex, resampler) to
**tens of GSa/s** (raw NCO phase accumulator).

To measure on your machine:

```bash
make benchmark          # Python-level; saves JSON to benchmarks/history/
make build              # then run C binaries directly, e.g.:
./build/native/src/fir/bench_fir_core
./build/native/src/hbdecim/bench_hbdecim_core
./build/native/src/resamp/bench_resamp_core
```

Historical Python benchmark snapshots are in
[`benchmarks/history/`](benchmarks/history/).

## Quick example

**C:**

```c
#include "fft/fft_core.h"
#include <complex.h>

dp_fft_t *fft = dp_fft_create(1024, -1, 1);
dp_cf32_t in[1024], out[1024];
/* ... fill in[] ... */
dp_fft_execute_cf32(fft, in, 1024, out);
dp_fft_destroy(fft);
```

**Python:**

```python
import numpy as np
from doppler.spectral import FFT

x = np.random.randn(1024).astype(np.complex64)
spectrum = FFT(1024).execute(x)
```

**DDC (tune a carrier to baseband):**

```python
from doppler.ddc import DDC
import numpy as np

ddc = DDC(norm_freq=-0.1, num_in=4096, rate=0.25)
x = np.random.randn(4096).astype(np.complex64)
y = ddc.execute(x)   # CF32, len ≈ 1024
```

## Documentation

**Full docs at [doppler-dsp.github.io/doppler](https://doppler-dsp.github.io/doppler/)**

| Document | Contents |
| -------- | -------- |
| [Quick Start](docs/quickstart.md) | Build, install, run the examples (Docker quickstart included) |
| [Build Guide](docs/build.md) | CMake options, platform notes, Python setup, Docker details |
| [Architecture](docs/architecture.md) | Design overview and layer diagram |
| [Examples: C](docs/examples/c.md) | C code examples — FFT, FIR, NCO, streaming |
| [Examples: Python](docs/examples/python.md) | Python code examples |
| [Examples: Streaming](docs/examples/streaming.md) | PUB/SUB and PUSH/PULL examples |

| [CLAUDE.md](CLAUDE.md) | Development notes and project context (for contributors) |

## Build

```bash
make          # build (Linux/macOS; MSYS2 on Windows)
make test     # run CTest suite
```

Or with Python bindings:

```bash
make pyext && uv sync
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
