# C Library

Build the library first (see [Build from Source](source.md)), then choose
one of the integration methods below.

## System install

Install headers and libraries to the system prefix (default `/usr/local`):

```sh
--8<-- "tests/install/cmake-install.sh:install"
```

Verify the install is visible to your toolchain:

```sh
--8<-- "tests/install/cmake-install.sh:verify"
```

### CMake — find_package

```cmake
find_package(doppler REQUIRED)
target_link_libraries(my_app PRIVATE doppler::doppler)
```

### pkg-config

```sh
gcc -o app main.c $(pkg-config --cflags --libs doppler)
```

!!! tip "Custom install prefix"

    ```sh
    --8<-- "tests/install/cmake-install.sh:custom-prefix"
    ```

## Link from the build tree (no install)

Useful during development — no `cmake --install` required.

**Shared library:**

```sh
--8<-- "tests/install/cmake-link.sh:shared"
```

!!! warning "rpath on Linux"

    The `-Wl,-rpath` flag bakes the build path into the binary. If you
    move the binary or the build tree, set `LD_LIBRARY_PATH` or re-link.

**Static library** (no runtime `.so` dependency):

```sh
--8<-- "tests/install/cmake-link.sh:static"
```

**CMake — add_subdirectory:**

```cmake
add_subdirectory(path/to/doppler)
target_link_libraries(my_app PRIVATE doppler::doppler)
```

## Embedding the `wfmgen` generator

The `wfmgen` composer CLI is archived into the library as a plain callable, so
a C program can drive the full generator in-process — same flags, same output
as the command line — without spawning a subprocess. Include `wfm/wfmgen.h`
and call `doppler_wfmgen(argc, argv)`:

```c
#include <stddef.h>
#include "wfm/wfmgen.h"

int main(void)
{
    /* identical to: wfmgen --type qpsk --count 4096 --output out.cf32 */
    char *av[] = { "wfmgen", "--type", "qpsk", "--count", "4096",
                   "--output", "out.cf32", NULL };
    return doppler_wfmgen(7, av);   /* 0 on success; writes out.cf32 */
}
```

It is the exact code path the `wfmgen` binary runs (which is itself a one-line
`main` shim over `doppler_wfmgen`), so the output is byte-identical. The zmq
PUB sink (`--output zmq://…`) is statically linked into the library, so there
is **no runtime `libzmq` dependency**.

`libdoppler` is **self-contained** — the vendored libzmq is built in, so neither
the shared nor the static library needs an external zmq. The static archive
links with just the library plus the C/C++ runtime:

```sh
gcc -o app app.c -I path/to/doppler/native/inc \
    libdoppler.a -lstdc++ -lpthread -lm
```

The **shared** library is even simpler — `-ldoppler` alone is sufficient.

!!! note "POSIX only"

    `doppler_wfmgen` is built on the same POSIX surface as the `wfmgen` binary
    (the zmq sink and `--realtime` pacing), so it is not available on Windows.
