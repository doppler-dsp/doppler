# Build from Source

## Prerequisites

| Tool | Minimum | Notes |
|------|---------|-------|
| CMake | 3.16 | |
| C compiler | C11 | GCC, Clang, or MSVC |
| C++ compiler | C++17 | Required for pocketfft (bundled) |

!!! note "Python extensions"
    `make pyext` additionally requires Python 3.11+ with development
    headers and NumPy.

### Ubuntu / Debian

```sh
--8<-- "tests/install/build-apt-deps.sh:install"
```

### macOS

```sh
--8<-- "tests/install/build-brew-deps.sh:install"
```

### Windows (MSYS2 — UCRT64 shell)

Install [MSYS2](https://www.msys2.org/) and open the **UCRT64** shell, then:

```sh
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-python \
          mingw-w64-ucrt-x86_64-python-numpy \
          make pkg-config
```

!!! warning "Use the UCRT64 shell, not MSYS"
    The MSYS POSIX compiler (`/usr/bin/cc`) and the UCRT64 native compiler
    (`/ucrt64/bin/cc`) have incompatible headers.  If CMake picks up the
    wrong one you'll see errors like `expected ';' before 'extern'` in
    `stddef.h`.  Always launch from the **UCRT64** shortcut so
    `/ucrt64/bin` is first on `PATH`, and delete any stale `build/`
    directory before reconfiguring.

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
    (AVX2, NEON, SVE, …) for the current machine.  Binaries built this
    way are **not portable** to other machines.

## CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_PYTHON` | `OFF` | Build Python C extensions (`make pyext` enables this) |
| `ENABLE_SIMD` | `ON` | SIMD / fast-math flags (disable for strict portability) |
| `Python3_EXECUTABLE` | auto | Override Python interpreter (ensures correct ABI suffix) |

Example — build Python extensions against a specific interpreter:

```sh
--8<-- "tests/install/build-source.sh:cmake-python"
```

## Make targets

| Target | Description |
|--------|-------------|
| `make` | Configure + build (Release) |
| `make pyext` | Build Python extensions into `src/doppler/` |
| `make test` | Run CTest suite (requires `make pyext` first) |
| `make python-test` | Run pytest |
| `make test-all` | CTest + pytest + example smoke tests |
| `make rust-test` | Build C library + run Rust FFI tests |
| `make debug` | Clean + Debug build |
| `make release` | Clean + Release build |
| `make blazing` | Clean + Release + `-march=native` |
| `make clean` | Remove `build/` and compiled `.so` files |
| `make help` | Show all targets and overrides |
