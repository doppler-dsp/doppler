# doppler -- Project Context for Claude

## Working Habits

- Update CLAUDE.md after each meaningful step
- Record active tasks and blockers so context survives session
  boundaries
- When starting a task, write it here; when finishing, mark it done
  or remove it

## Mission

```text
To provide an open-source, multi-language, cross-platform digital
signal processing library that is blazing fast and easy to use.
```

## Project Overview

Because `doppler` is first and foremost a **native C library** and
we care most about quality, performance and ease of use:

- We derive all other language extensions from the C ABI keeping
  wrappers as thin as possible.
- We add new functions to the C library first, then expose them to
  other languages via the C ABI.
- Full documentation of all APIs is standard before adding new
  capabilities.
  - At a minimum we add docstrings, examples and tests for all
    features.
  - Additional documentation in the form of markdown files
    describing the design, implementation and usage is often
    required for complex features.

The native C library is the core. Python et al. get native speed via
paper-thin bindings -- the Python layer does as little as possible
and delegates everything to C, while still feeling natural and easy
to use at the Python level. The goal is C performance with Python
ergonomics.

An FFI module ships example bindings and working implementations for
multiple languages (Rust, C++, and more), demonstrating how to
consume the C ABI cleanly from each ecosystem.

## Development Principles

These are hard-won lessons. Please take them seriously.

1. **One implementation, many wrappers.** Every algorithm, protocol,
   and data structure lives in the C library exactly once. Language
   bindings call the C functions -- they never reimplement logic.
   If you find yourself writing protocol parsing, header
   construction, or buffer management in Python/Rust/etc., stop:
   that code belongs in C, and you should be calling it.

2. **Shared wire formats.** There is one header struct
   (`dp_header_t`), one magic value, one framing convention. A C
   transmitter must be able to talk to a Python subscriber and vice
   versa. If a binding defines its own wire types, those will
   diverge and break interoperability -- this has happened before
   and required a full rewrite to fix.

3. **Thin means thin.** A language wrapper should be glue: type
   conversion, error translation, memory lifetime bridging. A good
   wrapper for a send function is 5-10 lines. If a wrapper file is
   growing past a few hundred lines, it is probably doing too much.

4. **Test at every layer.** C functions get C tests. Wrappers get
   their own integration tests that exercise the round-trip through
   the C library. Do not rely on wrapper tests to validate C logic,
   and do not skip wrapper tests because "the C tests pass."

5. **Build artifacts stay out of the source tree.** All cmake
   output goes to `build/`. Never run `cmake -B . -S c` or
   similar -- it scatters generated files into the repo root and
   they end up committed by accident.

## Formatting

- Line width: **79 characters** for all languages
- C: `clang-format` with the included `.clang-format` file
- Rust: `rustfmt` with the included `rustfmt.toml` file
- Python: `uvx ruff format --line-length=79`

## Naming Conventions

- **Project**: `doppler`
- **C API**: `dp_*` prefix (e.g. `dp_pub_create`,
  `dp_fft1d_execute`, `dp_c16_mul`)
- **Python**: `doppler` package
- **Sample types**: `DP_CI32`, `DP_CF64`, `DP_CF128`

## Directory Structure

```text
doppler/
├── c/
│   ├── include/
│   │   ├── doppler.h                # Umbrella header
│   │   └── dp/
│   │       ├── core.h          # Version, init
│   │       ├── stream.h        # Streaming (PUB/SUB, PUSH/PULL,
│   │       │                   #   REQ/REP)
│   │       ├── fft.h           # FFT (FFTW-backed)
│   │       ├── fir.h           # FIR filter (AVX-512/scalar)
│   │       ├── nco.h           # NCO (32-bit accumulator, 2^16 LUT)
│   │       ├── util.h          # SIMD utils (SSE2/scalar)
│   │       └── buffer.h        # Lock-free ring buffer
│   ├── src/                     # Implementations
│   ├── examples/                # transmitter, receiver,
│   │                            #   spectrum_analyzer,
│   │                            #   pipeline_demo, nco_demo, etc.
│   ├── tests/
│   │   ├── test_stream.c       # 26 streaming tests
│   │   ├── test_fir.c          # FIR unit tests
│   │   ├── test_nco.c          # 38 NCO unit tests
│   │   └── fft_testbench.c     # FFT round-trip tests
│   ├── bench/                   # Benchmarks
│   └── CMakeLists.txt
├── python/
│   ├── ext/
│   │   ├── _fft.c              # FFT C extension
│   │   ├── _buffer.c           # Buffer C extension
│   │   ├── _stream.c           # Streaming C extension
│   │   └── ...                 # _nco, _delay, _accumulator, etc.
│   └── doppler/               # Python package
│       ├── __init__.py
│       ├── fft/                 # FFT wrappers
│       └── tests/               # 54 pytest tests
├── ffi/rust/                    # Rust FFI bindings
├── docs/                        # All documentation (mkdocs site)
│   ├── build.md                # Build & install guide
│   ├── overview.md             # Architecture & API reference
│   ├── examples.md             # C & Python examples
│   ├── quickstart.md           # Getting started
│   ├── index.md → ../README.md # Symlink for mkdocs
│   ├── design/                 # Historical design docs
│   ├── api/                    # API reference pages
│   └── hooks.py                # Link-rewriting hook
├── .github/workflows/ci.yml
├── Makefile                     # Project wrapper
├── CHANGELOG.md                 # Development history
└── VENDORED.md                  # Vendoring policy
```

## Key APIs

### Streaming (`dp/stream.h`)

- Protocol: ZMQ multipart -- header frame (`SIGS` magic,
  dp_header_t) + data frame
- Patterns: PUB/SUB, PUSH/PULL, REQ/REP (raw bytes + signal
  frames)
- Zero-copy recv via `dp_msg_t` opaque handle
- Sample types: `DP_CI32`, `DP_CF64`, `DP_CF128`
- Version: 2.0.0

### Core DSP

- FFT: `dp_fft_global_setup`, `dp_fft1d_execute`, etc. (FFTW or
  pocketfft backend)
- FIR: `dp_fir_create`, `dp_fir_execute_*` (AVX-512 / scalar,
  real and complex taps, CI8/CI16/CI32/CF32 inputs)
- NCO: `dp_nco_create`, `dp_nco_execute_cf32`,
  `dp_nco_execute_cf32_ctrl` — 32-bit phase accumulator,
  2^16-entry sine LUT (~96 dBc SFDR), FM control port
- SIMD: `dp_c16_mul` (SSE2 on x86, scalar fallback)
- Buffer: `dp/buffer.h` -- lock-free SPSC ring buffer

## Build

```sh
make build          # cmake configure + build
make test           # CTest (2/2)
make pyext          # Build Python C extensions
make python-test    # pytest (54/54)
make install        # System install
make install-test   # 9-check post-install verification
make docs-build     # mkdocs build --strict
make blazing        # Release + -march=native (max CPU speed)
```

Extra cmake flags can be threaded through all configure steps:
```sh
make build CMAKE_ARGS="-DUSE_FFTW=OFF"
```

- Backend: cmake (C), uv (Python)
- Python >= 3.12, `uv sync` for dev deps

## Test Coverage

| Suite          | Count | Tool    |
|----------------|-------|---------|
| C streaming    | 26    | CTest   |
| C FFT          | 6     | CTest   |
| C FIR          | ?     | CTest   |
| C NCO          | 59    | CTest   |
| Python buffer  | 20    | pytest  |
| Python FFT     | 20    | pytest  |
| Python stream  | 14    | pytest  |
| Python NCO     | 26    | pytest  |
| Python DPMFS   | 40    | pytest  |
| Rust FFI       | 13    | cargo   |
| **Total**      | **224+**|       |

## Next Up

- specan (`doppler.specan`)
  - `make specan` / `uvx --from doppler-dsp doppler-specan` — shipped!
  - Move Hann window + FFT magnitude helper into the C library
  - Add frequency-domain peak annotation (label tone frequencies)
  - Interactive tone editor (click to add/remove/move tones)
  - Connect specan to real IQ source via doppler streaming (PUB/SUB)

- big feature: continuously variable resampler
  - NCO (done) + complex mixer + brickwall FIR = tunable filter
  - DPMFS resampler: C runtime done (`dp_resamp_dpmfs_*`)
  - Python design tools done: `fit_dpmfs`, `optimize_dpmfs`
  - Next: Python bindings for `dp_resamp_cf32` and `dp_resamp_dpmfs`
  - "tune and zoom" — any rate, any range, like an SDR frontend
  - specan is the visual front-end for this

- examples
  - Console outputs so users can see expected results
  - Benchmarks: C vs Rust-wrap-C vs rustfft
  - **DONE** (2026-03-27): Added C example for hbdecim, Python examples for nco & hbdecim,
    type stubs for dp_resamp & dp_resamp_dpmfs. All public C extensions now have
    type stubs. hbdecim C example registered in CMakeLists. Tests pass (167 pytest, 9 CTest).

- new features
  - NCO: AVX-512 execute path for cf32_ctrl (currently scalar)
