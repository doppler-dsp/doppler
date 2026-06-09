<p align="center">
  <img src="https://raw.githubusercontent.com/doppler-dsp/doppler/main/docs/assets/wordmark.png?v=3" alt="doppler — signal, shifted" width="560">
</p>

<p align="center"><strong>Dead-simple, ultra-fast digital signal processing.</strong></p>

<p align="center">
  <a href="https://github.com/doppler-dsp/doppler/actions/workflows/ci.yml"><img src="https://github.com/doppler-dsp/doppler/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="https://doppler-dsp.github.io/doppler/"><img src="https://img.shields.io/badge/docs-doppler--dsp.github.io-blue" alt="Docs"></a>
  <a href="https://pypi.org/project/doppler-dsp/"><img src="https://img.shields.io/pypi/v/doppler-dsp" alt="PyPI"></a>
  <a href="https://pypi.org/project/doppler-dsp/"><img src="https://img.shields.io/badge/python-3.9%20%E2%80%93%203.14-blue" alt="Python"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-green" alt="License: MIT"></a>
</p>

<p align="center">
  <a href="https://en.wikipedia.org/wiki/C99"><img src="https://img.shields.io/badge/C-C99-blue" alt="C99"></a>
  <a href="ffi/rust"><img src="https://img.shields.io/badge/Rust-FFI-CE4A00?logo=rust&logoColor=white" alt="Rust"></a>
  <a href="https://github.com/astral-sh/uv"><img src="https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/astral-sh/uv/main/assets/badge/v0.json" alt="uv"></a>
  <a href="https://github.com/astral-sh/ruff"><img src="https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/astral-sh/ruff/main/assets/badge/v2.json" alt="Ruff"></a>
</p>

doppler is a C99 DSP library: NCO, FIR filter, FFT, polyphase resampler,
DDC, and ZMQ-based signal streaming. Python and Rust wrap the same C core —
no second implementation, no divergence, full SIMD throughput from any
language.

## Why it's built this way

Every algorithm lives in C exactly once. The Python layer is type conversion
and lifetime bridging — a few hundred lines of glue, not a reimplementation.
Bugs get fixed once, benchmarks reflect real hardware, and a C transmitter
talks to a Python subscriber without surprises.

## Performance

On a Ryzen 7 AI 350 (`-O2`): NCO raw accumulator ~15 GSa/s, LO CF32
~1.8 GSa/s, FIR CF32 ~900 MSa/s, FFT CF32 (N=4096) ~180 MSa/s,
polyphase resampler (2× decim) ~70 MSa/s. Run `make bench` to measure
on your hardware.

## Quick start

**Python**

```python
from doppler.spectral import FFT
import numpy as np

X = FFT(1024).execute(np.random.randn(1024).astype(np.complex64))
```

**C**

```c
#include "fft/fft_core.h"

dp_fft_t *fft = dp_fft_create(1024, -1, 1);
dp_fft_execute_cf32(fft, in, 1024, out);
dp_fft_destroy(fft);
```

## Build

```bash
jbx install-deps        # install system deps (detects OS/distro)
make                    # C library
make pyext              # + Python bindings
make test               # CTest suite
make bench              # benchmarks
```

## Docs

Full docs at **[doppler-dsp.github.io/doppler](https://doppler-dsp.github.io/doppler/)** —
Quick Start, Architecture, API Reference, Examples.

## Licensing

MIT. The default build uses pocketfft (BSD-3-Clause). Build with
`-DUSE_FFTW=ON` for FFTW (LGPL).
