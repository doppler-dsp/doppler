# Build from Source

## Prerequisites

| Tool       | Minimum | Notes                                                                                                                                                                    |
| ---------- | ------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| CMake      | 3.16    |                                                                                                                                                                          |
| C compiler | C99     | GCC or Clang — builds the entire core library, including the optional NATS stream component (`libdoppler_stream`, vendors `nats.c`). No C++ compiler is needed anywhere. |

!!! note "Python extensions"

    `make pyext` additionally requires Python 3.9+ with development
    headers and NumPy.

### Ubuntu / Debian

```sh
--8<-- "tests/install/build-apt-deps.sh:install"
```

### Arch (incl. Manjaro, EndeavourOS, CachyOS)

```sh
--8<-- "tests/install/build-pacman-deps.sh:install"
```

### Fedora / RHEL (incl. Rocky, AlmaLinux)

```sh
--8<-- "tests/install/build-dnf-deps.sh:install"
```

### openSUSE (Leap / Tumbleweed)

```sh
--8<-- "tests/install/build-zypper-deps.sh:install"
```

### macOS

```sh
--8<-- "tests/install/build-brew-deps.sh:install"
```

### Windows

The **C library** builds natively on Windows and its full C test suite
passes, with either of clang's two drivers against the MSVC runtime:

| Compiler                       | How to get it                                                                                    |
| ------------------------------ | ------------------------------------------------------------------------------------------------ |
| `clang-cl` (MSVC-style driver) | Visual Studio's *C++ Clang tools for Windows* component, or [LLVM](https://llvm.org) for Windows |
| `clang` (GNU-style driver)     | the same install                                                                                 |

Both also need CMake 3.16+, Ninja, and the Windows SDK (installed with Visual
Studio's C++ workload). MSVC's own `cl.exe` **cannot** build doppler: it has
no C99 `_Complex`, and `native/inc/dp_complex.h` stops the build with an
`#error` saying so.

From a *Developer PowerShell for VS* (x64), which puts the SDK and linker on
the path:

```powershell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl
cmake --build build
ctest --test-dir build --output-on-failure
```

This is the build `.github/workflows/windows.yml` runs, once per driver, on
every push to `main` and on pull requests that touch the C. That workflow is **non-binding** and publishes nothing: there are no
Windows wheels or tarballs. On Windows the static library is
`doppler_static.lib`, because `doppler.lib` is already the DLL's import
library. The CMake target names (`doppler::doppler`, `doppler::doppler-static`)
are the same on every platform.

**Not available on Windows yet.** These are not ported, and CMake leaves them
out of a Windows build rather than failing:

- the NATS stream layer (`libdoppler_stream`, `dp_pub_*` / `dp_sub_*`), and
    everything built on it: the wfm `StreamSink`, the `wfmgen` CLI and
    `doppler_wfmgen`, and the streaming examples;
- the Python extensions and the Rust crate, which are built and tested on
    Linux and macOS only. For Python on Windows, use
    [WSL2](https://learn.microsoft.com/windows/wsl/), a Linux VM, or a
    container, and follow the Ubuntu / Debian steps above;
- a few tests of POSIX-only behaviour (signal chaining in `dp_interrupt`, a
    memory soak that reads `getrusage`).

Each carve-out is tracked on
[#1364](https://github.com/doppler-dsp/doppler/issues/1364).

## Build

```sh
--8<-- "tests/install/build-source.sh:make"
```

Or directly with CMake:

```sh
--8<-- "tests/install/build-source.sh:cmake"
```

!!! tip "Maximum performance"

    `make blazing` adds `-march=native` to enable all CPU extensions
    (AVX2, NEON, SVE, …) for the current machine. Binaries built this
    way are **not portable** to other machines.

## CMake options

| Option               | Default | Description                                              |
| -------------------- | ------- | -------------------------------------------------------- |
| `BUILD_PYTHON`       | `OFF`   | Build Python C extensions (`make pyext` enables this)    |
| `ENABLE_SIMD`        | `ON`    | SIMD / fast-math flags (disable for strict portability)  |
| `Python3_EXECUTABLE` | auto    | Override Python interpreter (ensures correct ABI suffix) |

Example — build Python extensions against a specific interpreter:

```sh
--8<-- "tests/install/build-source.sh:cmake-python"
```

## Make targets

| Target             | Description                                   |
| ------------------ | --------------------------------------------- |
| `make`             | Configure + build (Release)                   |
| `make pyext`       | Build Python extensions into `src/doppler/`   |
| `make test`        | Run CTest suite (requires `make pyext` first) |
| `make test-python` | Run pytest                                    |
| `make test-all`    | CTest + pytest + example smoke tests          |
| `make test-rust`   | Build C library + run Rust FFI tests          |
| `make bench`       | Run C + Python benchmarks on this machine     |
| `make debug`       | Clean + Debug build                           |
| `make release`     | Clean + Release build                         |
| `make blazing`     | Clean + Release + `-march=native`             |
| `make clean`       | Remove `build/` and compiled `.so` files      |
| `make help`        | Show all targets and overrides                |
