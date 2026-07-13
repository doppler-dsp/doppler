# ![doppler — signal, shifted](https://raw.githubusercontent.com/doppler-dsp/doppler/main/docs/assets/wordmark.png?v=3) { .md-home-wordmark }

<p align="center"><strong>Practical, portable, performant digital signal processing.</strong></p>

<p align="center">
  <a href="https://github.com/doppler-dsp/doppler/actions/workflows/ci.yml"><img class="off-glb" src="https://github.com/doppler-dsp/doppler/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="https://doppler-dsp.github.io/doppler/"><img class="off-glb" src="https://img.shields.io/badge/docs-doppler--dsp.github.io-blue" alt="Docs"></a>
  <a href="https://pypi.org/project/doppler-dsp/"><img class="off-glb" src="https://img.shields.io/pypi/v/doppler-dsp" alt="PyPI"></a>
  <a href="https://pypi.org/project/doppler-dsp/"><img class="off-glb" src="https://img.shields.io/badge/python-3.9%20%E2%80%93%203.14-blue" alt="Python"></a>
  <a href="LICENSE"><img class="off-glb" src="https://img.shields.io/badge/license-MIT-green" alt="License: MIT"></a>
</p>

<p align="center">
  <a href="https://en.wikipedia.org/wiki/C99"><img class="off-glb" src="https://img.shields.io/badge/C-C99-blue" alt="C99"></a>
  <a href="install/rust.md"><img class="off-glb" src="https://img.shields.io/badge/Rust-FFI-CE4A00?logo=rust&logoColor=white" alt="Rust"></a>
  <a href="https://github.com/astral-sh/uv"><img class="off-glb" src="https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/astral-sh/uv/main/assets/badge/v0.json" alt="uv"></a>
  <a href="https://github.com/astral-sh/ruff"><img class="off-glb" src="https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/astral-sh/ruff/main/assets/badge/v2.json" alt="Ruff"></a>
</p>

doppler is a C99 DSP library — NCO, FIR, FFT, polyphase resampling, DDC,
AGC and more — with file, buffer, and NATS-based streaming, and a
scenario-driven waveform generator (`wfmgen`) with byte-identical
CLI/Python/C parity. Python and Rust wrap the same C core — no second
implementation, no divergence, full SIMD throughput from any language.

**New here?** Start with [Start Here](start-here.md) — a one-page map from
"what are you trying to do" to the right doc.

**Navigate** — [Quick Start](quickstart.md) · [Architecture](architecture.md) · [Gallery](gallery/index.md) · [Examples](examples/index.md) · [Guides](guide/index.md) · [Waveform Generator](guide/wfmgen/index.md) · [Design](design/index.md) · [Contributing](dev/index.md)

**API Reference** — [Full Python + C API index](api/index.md)

______________________________________________________________________

## Why it's built this way

Every algorithm lives in C exactly once. The Python layer is type conversion
and lifetime bridging — a few hundred lines of glue, not a reimplementation.
Bugs get fixed once, benchmarks reflect real hardware, and a C transmitter
talks to a Python subscriber without surprises.

## Performance

On a Ryzen 7 AI 350 (`-O2`): NCO raw accumulator ~15 GSa/s, LO CF32
~1.8 GSa/s, FIR CF32 ~900 MSa/s. The full generated table lives in
[Benchmarks](benchmarks.md); run
`make bench` to measure on your hardware.

## Quick start

See [Quick Start](quickstart.md) for the full walkthrough.

### Python

**Install**

!!! note

    Isolate your install from system python with a virtual environment!

    ```bash
    python3 -m venv .venv
    . .venv/bin/activate
    ```

```bash
pip install doppler-dsp
```

**Compute FFT**

```python
from doppler.spectral import FFT
import numpy as np

x = np.random.randn(1024).astype(np.complex64)
X = FFT(1024).execute_cf32(x)
print(f"FFT: {len(x)} samples in -> {X.shape[0]} complex64 bins out")
```

**Create a Waveform**

```python
from doppler.wfm import Synth

synth = Synth(type="qpsk", fs=1e6, snr=12.0, snr_mode="esno", sps=8, seed=1)
iq = synth.steps(4096)   # complex64 ndarray
print(f"generated {len(iq)} QPSK samples")
```

### C

!!! tip "Don't have `jbx` yet?"

    ```bash
    . <(curl -sSL https://just-buildit.github.io/get-jb.sh)
    ```

**Install**

Get `libdoppler.a`/`libdoppler.so` plus headers, ready to link, in one command:

```bash
jbx get-doppler
```

**Compute FFT**

```c
/* example.c */

#include <complex.h>
#include <stdio.h>
#include <fft/fft_core.h>

int main(void)
{
  float complex in[1024]  = { 0 };   /* fill with your samples */
  float complex out[1024];

  fft_state_t *fft = fft_create(1024, -1, 1);  /* n, sign, nthreads */
  fft_execute_cf32(fft, in, 1024, out);        /* in,out: float complex[1024] */
  fft_destroy(fft);
  printf("FFT: 1024 samples in -> 1024 complex bins out\n");
  return 0;
}
```

**Compile and run**

```bash
cc example.c -I "$HOME/.local/doppler/include" "$HOME/.local/doppler/lib/libdoppler.a" -lm -o example
./example
```

**Other install methods**

Prefer a custom prefix or no `jbx`? Grab a
[pre-built release tarball](install/c.md#install-from-a-release-tarball) by
hand — no toolchain, no building doppler itself — and extract it to
`$PREFIX`; you get the same `libdoppler.a`/`libdoppler.so` plus headers.
See [C Library](install/c.md) for `find_package`/`pkg-config` integration
and building from source.

## Build

Building from source gets you the C library, examples, and Rust FFI
bindings — see [Build from source](quickstart.md#build-from-source) if you
just want the C library without cloning the repo.

```bash
git clone https://github.com/doppler-dsp/doppler
cd doppler
make install-deps       # bootstrap jbx (if needed) + install system deps
make                    # C library
make pyext              # + Python bindings
make test               # CTest suite
make bench              # benchmarks
```

## Docs

Full docs: **[doppler-dsp.github.io/doppler](https://doppler-dsp.github.io/doppler/)**

## Licensing

MIT. The core C library is pure C99 and links only `-lm`. Its FFT uses the
vendored pocketfft (BSD-3-Clause) for double precision and arbitrary sizes, and
the vendored PFFFT (Pommier/FFTPACK, BSD) for the native single-precision SIMD
path. The optional NATS stream component (`libdoppler_stream`) vendors
`nats.c` (Apache-2.0) — it too is pure C99, so no C++ toolchain is needed
anywhere in the build.
