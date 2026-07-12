<p align="center">
  <img src="https://raw.githubusercontent.com/doppler-dsp/doppler/main/docs/assets/wordmark.png?v=3" alt="doppler — signal, shifted" width="560">
</p>

<p align="center"><strong>Practical, portable, performant digital signal processing.</strong></p>

<p align="center">
  <a href="https://github.com/doppler-dsp/doppler/actions/workflows/ci.yml"><img src="https://github.com/doppler-dsp/doppler/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="https://doppler-dsp.github.io/doppler/"><img src="https://img.shields.io/badge/docs-doppler--dsp.github.io-blue" alt="Docs"></a>
  <a href="https://pypi.org/project/doppler-dsp/"><img src="https://img.shields.io/pypi/v/doppler-dsp" alt="PyPI"></a>
  <a href="https://pypi.org/project/doppler-dsp/"><img src="https://img.shields.io/badge/python-3.9%20%E2%80%93%203.14-blue" alt="Python"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-green" alt="License: MIT"></a>
</p>

<p align="center">
  <a href="https://en.wikipedia.org/wiki/C99"><img src="https://img.shields.io/badge/C-C99-blue" alt="C99"></a>
  <a href="install/rust.md"><img src="https://img.shields.io/badge/Rust-FFI-CE4A00?logo=rust&logoColor=white" alt="Rust"></a>
  <a href="https://github.com/astral-sh/uv"><img src="https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/astral-sh/uv/main/assets/badge/v0.json" alt="uv"></a>
  <a href="https://github.com/astral-sh/ruff"><img src="https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/astral-sh/ruff/main/assets/badge/v2.json" alt="Ruff"></a>
</p>

doppler is a C99 DSP library — NCO, FIR, FFT, polyphase resampling, DDC,
AGC and more — with file, buffer, and NATS-based streaming, and a
scenario-driven waveform generator (`wfmgen`) with byte-identical
CLI/Python/C parity. Python and Rust wrap the same C core — no second
implementation, no divergence, full SIMD throughput from any language.

**Navigate** — [Quick Start](quickstart.md) · [Architecture](architecture.md) · [Examples](examples/index.md) · [Guides](guide/index.md) · [Waveform Generator](guide/wfmgen/index.md)

**API Reference** — [C API](c-api/files.md) · [Python: FFT](api/python-fft.md) · [Python: Spectral](api/python-spectral.md) · [Python: Waveform](api/python-wfmgen.md) · [Python: Resample](api/python-resample.md) · [Python: Filter](api/python-filter.md) · [Python: Measurement](api/python-measure.md) · [Python: DDC](api/python-ddc.md) · [Python: Accumulator](api/python-accumulator.md)

______________________________________________________________________

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

**Python** — `pip install doppler-dsp` (bundles all native deps, no system
libraries needed); see [Quick Start](quickstart.md) for the full walkthrough.

```python
from doppler.spectral import FFT
import numpy as np

x = np.random.randn(1024).astype(np.complex64)
X = FFT(1024).execute_cf32(x)
```

**Waveform**

```python
from doppler.wfm import Synth

synth = Synth(type="qpsk", fs=1e6, snr=12.0, snr_mode="esno", sps=8, seed=1)
iq = synth.steps(4096)   # complex64 ndarray
```

**C** — the C library isn't part of the `pip install` wheel. Grab a
[pre-built release tarball](install/c.md#install-from-a-release-tarball) —
no toolchain, no building doppler itself — and extract it to `$PREFIX`
(`jbx get-doppler` does exactly this in one command); you get
`libdoppler.a`/`libdoppler.so` plus headers, ready to link.
([Building from source](quickstart.md#build-from-source) instead? See
that section — it uses `build/` tree paths, not `$PREFIX`.) See
[C Library](install/c.md) for `find_package`/`pkg-config` integration.

```c
#include <complex.h>
#include <fft/fft_core.h>

int main(void)
{
  float complex in[1024]  = { 0 };   /* fill with your samples */
  float complex out[1024];

  fft_state_t *fft = fft_create(1024, -1, 1);  /* n, sign, nthreads */
  fft_execute_cf32(fft, in, 1024, out);        /* in,out: float complex[1024] */
  fft_destroy(fft);
  return 0;
}
```

```bash
# after extracting the tarball to $PREFIX (install/c.md) --
# find_package/pkg-config integration there too
cc example.c -I "$PREFIX/include" "$PREFIX/lib/libdoppler.a" -lm -o example
```

## Build

> [!TIP]
> Don't have `jbx` yet? `make install-deps` bootstraps it for you. (Or by
> hand: `. <(curl -sSL https://just-buildit.github.io/get-jb.sh)`.)

```bash
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
path. The optional stream component (`libdoppler_stream`) vendors `nats.c`
(Apache-2.0).
