# Build and Installation Guide

See [README](../README.md) for an overview and quick examples.

## Quick build

```sh
make
```

Produces the following artifacts in `build/` (extensions differ by platform):

| Artifact | Linux/macOS | Windows (MinGW) | Description |
| -------- | ----------- | --------------- | ----------- |
| Shared Library | `libdoppler.so/dylib` | `libdoppler.dll` | DSP + streaming |
| Static Library | `libdoppler.a` | `libdoppler.a` | Static link (no runtime dep) |
| Python extension | `fft/_fft*.so` | `fft/_fft*.pyd` | FFT |
| Python extension | `buffer/_buffer*.so` | `buffer/_buffer*.pyd` | Lock-free ring buffer |
| Python extension | `stream/_stream*.so` | `stream/_stream*.pyd` | ZMQ streaming (vendored libzmq) |
| Python extension | `nco/_nco*.so` | `nco/_nco*.pyd` | NCO |
| Python extension | `accumulator/_accumulator*.so` | `accumulator/_accumulator*.pyd` | Accumulator |
| Python extension | `delay/_delay*.so` | `delay/_delay*.pyd` | Delay line |
| Python extension | `resample/_resamp*.so` | `resample/_resamp*.pyd` | Polyphase resampler |
| Python extension | `resample/_resamp_dpmfs*.so` | `resample/_resamp_dpmfs*.pyd` | DPMFS resampler |
| Python extension | `resample/_hbdecim*.so` | `resample/_hbdecim*.pyd` | Halfband decimator |
| C examples | `transmitter`, `receiver`, … | `transmitter.exe`, … | Streaming and DSP demos |
| Rust examples | `ffi/rust/target/…/nco_demo`, … | same | NCO, FFT, SIMD demos |

And the top level Python package containing the above modules is
in `dist/`: `doppler_dsp-*.whl` & `doppler_dsp-*.tar.gz`

### Targets

| Target | Description |
| ------ | ----------- |
| `make` | Configure + build (Release by default) |
| `make test` | Run CTest suite |
| `make test-all` | Run all test suites (C + Python + Rust) |
| `make pyext` | Build Python extensions into `python/dsp/doppler/` |
| `make install` | Install headers + libs to system (default `/usr/local`) |
| `make python-test` | Run pytest |
| `make rust-test` | Run Rust FFI tests (single-threaded) |
| `make rust-examples` | Build Rust examples and list their paths |
| `make docker` | Build Docker image |
| `make docker-test` | Build image + run container tests |
| `make debug` | Clean + Debug build |
| `make release` | Clean + Release build |
| `make clean` | Remove build artifacts |
| `make help` | Show all targets and overrides |

## Python bindings

The Python package is a standard pip-installable package:

```bash
pip install doppler
```

**From source** — build and install the C library first, then install the Python package:

```bash
make && sudo make install
pip install python/
```

The wheel contains nine compiled extension modules, each installed inside
its own subpackage (`doppler.fft`, `doppler.nco`, `doppler.buffer`,
`doppler.stream`, `doppler.accumulator`, `doppler.delay`,
`doppler.resample`).  The streaming extension statically links vendored
libzmq — no system packages required at runtime.

## CMake directly

```bash
cmake -B build -S c -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build
```

## CMake options

| Option | Default | Description |
| ------ | ------- | ----------- |
| `USE_FFTW` | ON | FFTW3 backend (default). OFF = pocketfft (MIT-only) |
| `NumPy_INCLUDE_DIR` | auto | Override NumPy include path (e.g. from a uv venv) |
| `Python3_EXECUTABLE` | auto | Override Python interpreter (ensures correct ABI suffix) |

### FFT backend

To use the bundled pocketfft backend instead of FFTW3:

```bash
cmake -DUSE_FFTW=OFF -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

See [README — Licensing](../README.md#licensing) for the implications of each choice.
License texts: [FFTW_LICENSE](../c/cmake/licenses/FFTW_LICENSE) · [POCKETFFT_LICENSE](../c/cmake/licenses/POCKETFFT_LICENSE).

## Dependencies

### C Library Dependencies

All dependencies are available via the standard package manager on each platform.

| Dependency | Version | Ubuntu/Debian | macOS (Homebrew) | Windows (MSYS2) |
| ---------- | ------- | ------------- | ---------------- | --------------- |
| ZeroMQ | ≥ 4.3 | `libzmq3-dev` | `zeromq` | `mingw-w64-x86_64-zeromq` |
| FFTW3 | ≥ 3.3 | `libfftw3-dev` | `fftw` | `mingw-w64-x86_64-fftw` |
| CMake | ≥ 3.16 | `cmake` | `cmake` | `mingw-w64-x86_64-cmake` |
| Python | ≥ 3.12 | `python3-dev` | `python` | `mingw-w64-x86_64-python` |
| NumPy | ≥ 2.4 | `python3-numpy` | `numpy` | `mingw-w64-x86_64-python-numpy` |

**Ubuntu/Debian:**

```bash
sudo apt-get install libzmq3-dev libfftw3-dev cmake pkg-config python3-dev python3-numpy
```

**macOS** (all deps available via [Homebrew](https://brew.sh)):

```bash
brew install zeromq fftw cmake python numpy
```

**Windows (MSYS2 / UCRT64):**

Install [MSYS2](https://www.msys2.org/) and open the **UCRT64** shell
(not MSYS2, not MinGW64 — the UCRT64 environment uses the modern
Universal C Runtime and avoids header-mixing issues).

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-zeromq \
          mingw-w64-ucrt-x86_64-fftw \
          mingw-w64-ucrt-x86_64-python \
          mingw-w64-ucrt-x86_64-python-numpy \
          mingw-w64-ucrt-x86_64-rust \
          make pkg-config
```

> **Why UCRT64?** The MSYS POSIX environment (`/usr/bin/cc`) and the
> UCRT64 native environment (`/ucrt64/bin/cc`) have incompatible headers.
> If cmake picks up the wrong `cc` you'll see errors like
> `expected ';' before 'extern'` in `stddef.h`. Always launch from the
> UCRT64 shortcut so `/ucrt64/bin` is first on `PATH`, and clear any
> stale `build/` directory before reconfiguring.

### Python Extension: Vendored Dependencies

The Python `dp_stream` extension statically links a **vendored copy of libzmq** to eliminate runtime dependencies. This means `pip install doppler` requires **no system packages** on the user's machine.

| Dependency | Version | Location | License | Why Vendored |
| ---------- | ------- | -------- | ------- | ------------ |
| **libzmq** | 4.3.5 | `python/vendor/libzmq/` | MPL-2.0 | Static link in Python extension for zero-dependency installs |
| **pocketfft** | - | `c/src/pocketfft.cc` | BSD-3-Clause | Fallback FFT backend when FFTW not available |

**For developers:** Vendored libzmq is built automatically when you run `make pyext`. No manual steps needed.

**For users:** `pip install doppler` includes the vendored libzmq (statically linked). You don't need to install `libzmq-dev` or any other system packages.

**See:** [VENDORED.md](../VENDORED.md) for vendoring policy, update procedures, and licensing details.

## Using in your project

### From a system install (`make install`)

**CMake** (`find_package`):

```cmake
find_package(doppler REQUIRED)
target_link_libraries(my_app PRIVATE doppler::doppler)
```

**pkg-config:**

```bash
gcc -o app main.c $(pkg-config --cflags --libs doppler)
```

Verify the install is visible to your toolchain:

```bash
pkg-config --modversion doppler
```

**Python:**

```bash
pip install doppler
```

### Linking directly from the build tree (no install)

**Shared library:**

```bash
gcc -o app main.c \
  -Ic/include \
  -Lbuild -ldoppler \
  $(pkg-config --libs libzmq) \
  -Wl,-rpath,$(pwd)/build
```

**Static library** (no runtime `.so` dependency):

```bash
gcc -o app main.c \
  -Ic/include \
  build/libdoppler.a \
  $(pkg-config --libs libzmq) -lm
```

**CMake (add_subdirectory):**

```cmake
add_subdirectory(path/to/doppler/c)
target_link_libraries(my_app PRIVATE doppler)
```

## Rust FFI bindings

The `ffi/rust/` crate provides idiomatic Rust wrappers around the C library.
It requires no extra Rust dependencies beyond `num-complex` — all DSP logic
runs in the C library.

### Prerequisites

- Rust toolchain — install via the MSYS2 package manager on Windows
  (`mingw-w64-ucrt-x86_64-rust`) or via [rustup](https://rustup.rs/)
  on Linux/macOS
- The C library built first: `make build`

### Build and test

```sh
make rust-test       # build C library + run all 33 Rust tests
make rust-examples   # build examples and list their paths
```

**Linux/macOS** — `build.rs` bakes an rpath into every binary so
examples run directly without setting `LD_LIBRARY_PATH`:

```sh
./ffi/rust/target/debug/examples/nco_demo
./ffi/rust/target/debug/examples/fft_demo
./ffi/rust/target/debug/examples/acc_demo
```

**Windows (UCRT64)** — the Rust crate links `libdoppler.a` statically,
so there is no `libdoppler.dll` runtime dependency. Examples run
directly from the UCRT64 shell:

```sh
./ffi/rust/target/debug/examples/nco_demo.exe
./ffi/rust/target/debug/examples/fft_demo.exe
```

`zmq.dll` and `fftw3.dll` are still loaded dynamically; they live in
`/ucrt64/bin/` which is on `PATH` by default in the UCRT64 shell.

The rpath always points at `build/c/`. After `make install`, use the
installed system library by passing `-DCMAKE_SKIP_RPATH=ON` or by
running `cargo build` with `PKG_CONFIG_PATH` set to the install prefix.

### Modules

| Module | Wraps | Description |
| ------ | ----- | ----------- |
| `acc` | `dp_acc_f32_*`, `dp_acc_cf64_*` | f32 and cf64 accumulators |
| `fft` | `dp_fft_global_setup`, `dp_fft1d/2d_execute` | 1-D and 2-D FFT |
| `fir` | `dp_fir_*` | FIR filter (complex and real taps) |
| `nco` | `dp_nco_*` | Numerically-controlled oscillator |
| `util::c16_mul` | `dp_c16_mul` | SIMD complex multiplication |

### Sample types

The crate exposes `#[repr(C)]` structs that match the C ABI exactly:

| Rust type | C type | Description |
| --------- | ------ | ----------- |
| `DpCf32` | `dp_cf32_t` | Complex f32 (I+Q) — converts to/from `Complex<f32>` |
| `DpCi8` | `dp_ci8_t` | Complex i8 |
| `DpCi16` | `dp_ci16_t` | Complex i16 |
| `DpCi32` | `dp_ci32_t` | Complex i32 |

### Using from another crate

Point Cargo at the local path:

```toml
[dependencies]
doppler = { path = "path/to/doppler/ffi/rust" }
```

Or after publishing to crates.io:

```toml
[dependencies]
doppler = "0.1"
```

## Docker

Build and run the library with all dependencies in a container:

```bash
# Build image (~130 MB, includes all examples and tests)
docker build -t doppler .

# Run unit tests
docker run --rm doppler /app/test_stream
```

Expected test output:

```text
========================================
doppler Library Unit Tests
========================================

Test 1: Initialization and cleanup... PASS
Test 2: Sample type utilities... PASS
Test 3: Timestamp generation... PASS
Test 4: Error code strings... PASS
Test 5: Publisher send without subscriber... PASS
Test 6: Publisher-Subscriber communication... PASS (received 5 packets, 500 samples)
Test 7: CI32 data type... PASS
Test 8: Multiple subscribers... PASS

========================================
Results: 8/8 tests passed
========================================
```

### Available binaries in the image

| Binary | Description |
| ------ | ----------- |
| `transmitter` | Generates and publishes signal samples over ZMQ PUB |
| `receiver` | Subscribes and prints signal stats |
| `spectrum_analyzer` | ASCII real-time spectrum display |
| `pipeline_demo` | PUSH/PULL pipeline demo (in-process threads) |
| `fft_demo` | FFT demonstration |
| `test_stream` | Streaming unit tests |
| `fft_testbench` | FFT correctness tests |

### Docker Compose

Run a complete streaming demo with transmitter, receivers, and spectrum analyzer:

```bash
# Start all services (transmitter + 2 receivers + spectrum analyzer)
docker compose up

# Or run just the tests
docker compose --profile test run --rm tests
```

Example receiver output:

```text
doppler Receiver
===============
Endpoint: tcp://transmitter:5555

Waiting for data... Press Ctrl+C to stop.

First packet received:
  Sample Type:  CF64
  Num Samples:  8192
  Sample Rate:  1.00 MHz
  Center Freq:  2.40 GHz
  Sequence:     0
  Power:        0.00 dB
First 5 samples:
  [0] I: 1.000000, Q: 0.000000
  [1] I: 0.998027, Q: 0.062791
  [2] I: 0.992115, Q: 0.125333
  [3] I: 0.982287, Q: 0.187381
  [4] I: 0.968583, Q: 0.248690
```

The Dockerfile:

- Multi-stage build (builder + runtime) for minimal image size
- Builds with `-j$(nproc)` parallel compilation
- Runtime stage includes only `libzmq5` and `libfftw3-3`
- All example binaries, streaming library, and DSP library are included
- `STOPSIGNAL SIGTERM` for clean shutdown of signal-handling binaries
- Exposes port 5555 for ZMQ streaming
