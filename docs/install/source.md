# Build from Source

## Prerequisites

| Tool         | Minimum | Notes                                                       |
| ------------ | ------- | ----------------------------------------------------------- |
| CMake        | 3.16    |                                                             |
| C compiler   | C99     | GCC or Clang — builds the entire core library               |
| C++ compiler | C++17   | **Optional** — only for the ZMQ/stream component (vendored libzmq is C++); the core libdoppler is pure C99 |

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

!!! info "Windows"

    doppler does not target Windows natively. Build and run under
    [WSL2](https://learn.microsoft.com/windows/wsl/), a Linux VM, or a
    container, and follow the Ubuntu / Debian steps above.

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
| `make python-test` | Run pytest                                    |
| `make test-all`    | CTest + pytest + example smoke tests          |
| `make rust-test`   | Build C library + run Rust FFI tests          |
| `make debug`       | Clean + Debug build                           |
| `make release`     | Clean + Release build                         |
| `make blazing`     | Clean + Release + `-march=native`             |
| `make clean`       | Remove `build/` and compiled `.so` files      |
| `make help`        | Show all targets and overrides                |
