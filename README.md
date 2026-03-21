# doppler

**Dead-simple, ultra-fast, digital signal processing.**

doppler is a high‑performance signal processing library laser focused on maximizing both speed and ease of use -- __from any language__. Built on a lean portable C99 base with pluggable FFT backend (FFTW or pocketfft), AVX2 SIMD kernels, and ZMQ-based streaming transport. Fully cross-platform: Linux, macOS, and Windows (MinGW-w64).

## What's inside

- **FFT** — 1D and 2D, selectable backend (FFTW or pocketfft)
- **SIMD arithmetic** — AVX2 complex multiply via `dp_c16_mul`
- **Signal streaming** — low-latency ZMQ transport (PUB/SUB, PUSH/PULL, REQ/REP)
- **Circular buffers** — double-mapped ring buffers for zero-copy, lock-free IPC (F32/F64/I16)
- **Extensible sample types** — CI32, CF64, CF128 today; more as needed
- **Multi-language** — clean C ABI; Python bindings (FFT + streaming + buffers) and Rust FFI included

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
