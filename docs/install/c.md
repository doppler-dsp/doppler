# C Library

Get the C library one of two ways — **a pre-built release tarball** (no build)
or **[building from source](source.md)** — then choose an integration method
below. Either way you get the headers plus a shared (`libdoppler.so`) and a
self-contained static (`libdoppler.a`) library.

## Install from a release tarball

Every [GitHub release](https://github.com/doppler-dsp/doppler/releases) ships a
pre-built C library for Linux (x86_64) and macOS (arm64) — no toolchain or
build step.

!!! tip "One-liner via jbx"

    ```sh
    jbx get-doppler                          # extracts to $HOME/doppler
    jbx get-doppler --prefix /opt/doppler    # or a custom prefix
    jbx get-doppler --version 0.33.1         # pin a specific release
    ```

    Resolves the latest release, downloads the platform-appropriate tarball,
    and extracts it — the manual steps below, in one command. Needs
    [`jbx`](https://just-buildit.github.io/) (`make install-deps` bootstraps
    it, or by hand: `. <(curl -sSL https://just-buildit.github.io/get-jb.sh)`).
    Source: [`scripts/get-doppler.sh`](https://github.com/doppler-dsp/doppler/blob/main/scripts/get-doppler.sh).

Or by hand:

```sh
# Resolve the latest release tag (or set VERSION=x.y.z to pin a specific one):
VERSION=$(curl -fsSL https://api.github.com/repos/doppler-dsp/doppler/releases/latest \
  | sed -n 's/.*"tag_name": *"v\([^"]*\)".*/\1/p')
# Linux x86_64 — for macOS arm64, swap the suffix for `macos-arm64`:
curl -L -o doppler.tar.gz \
  "https://github.com/doppler-dsp/doppler/releases/download/v${VERSION}/doppler-${VERSION}-linux-x86_64.tar.gz"
mkdir -p "$HOME/doppler" && tar -xzf doppler.tar.gz -C "$HOME/doppler"
```

Point your build at the extracted prefix — CMake via `CMAKE_PREFIX_PATH`, or
pkg-config via `PKG_CONFIG_PATH` — then use the **find_package** or
**pkg-config** method shown below:

```sh
cmake -B build -DCMAKE_PREFIX_PATH="$HOME/doppler"     # for find_package(doppler)
export PKG_CONFIG_PATH="$HOME/doppler/lib/pkgconfig"   # for pkg-config doppler
```

The core library is pure C and links only `-lm`, so there is no external
runtime dependency to install. The optional stream component
(`libdoppler_stream`, for the NATS wire layer) embeds the vendored `nats.c`
statically, so it too needs no external runtime NATS client library — just a
running `nats-server` to connect to. See the
[static vs. dynamic linking design notes](../design/archive/STATIC_VS_DYNAMIC.md)
for why static vendoring was chosen over a system client-library dependency.

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

The package provides two link targets — pick the one matching your linking
policy:

```cmake
find_package(doppler REQUIRED)

# shared: links -ldoppler; smallest binary
target_link_libraries(my_app PRIVATE doppler::doppler)

# static: pure C, self-contained — links only -lm
target_link_libraries(my_app PRIVATE doppler::doppler-static)

# optional: the NATS stream layer (dp_pub_*/dp_sub_*, wfmgen --output nats://).
# Pure C (vendored nats.c, folded in). Link only if needed.
target_link_libraries(my_app PRIVATE doppler::stream-static)
```

A complete, buildable consumer that exercises both targets lives in
[`examples/consumer/`](https://github.com/doppler-dsp/doppler/tree/main/examples/consumer).

### pkg-config

```sh
# shared
gcc -o app main.c $(pkg-config --cflags --libs doppler)

# static — pure C, self-contained: only -lm
gcc -o app main.c $(pkg-config --cflags doppler) \
    "$(pkg-config --variable=libdir doppler)/libdoppler.a" -lm

# static + the optional NATS stream layer (pure C, no extra runtime)
gcc -o app main.c $(pkg-config --cflags doppler) \
    "$(pkg-config --variable=libdir doppler)/libdoppler.a" \
    "$(pkg-config --variable=libdir doppler)/libdoppler_stream.a" \
    -lpthread -lm
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
`main` shim over `doppler_wfmgen`), so the output is byte-identical. `wfmgen`
lives in the **pure-C core**, so the file/raw/csv/BLUE/SigMF output paths link
with just `libdoppler.a -lm`:

```sh
gcc -o app app.c -I "$PREFIX/include" "$PREFIX/lib/libdoppler.a" -lm
```

The `--output nats://…` PUB-sink path additionally needs the optional stream
component (it carries the vendored `nats.c`, pure C). Link
`libdoppler_stream.a` alongside the core; `nats.c` is statically embedded, so
there is still **no runtime client-library dependency** — just a running
`nats-server` to connect to:

```sh
gcc -o app app.c -I "$PREFIX/include" \
    "$PREFIX/lib/libdoppler.a" "$PREFIX/lib/libdoppler_stream.a" \
    -lpthread -lm
```

Without the stream component, `doppler_wfmgen()` still builds and runs; only the
`nats://` path is unavailable (it reports a clear "requires the stream component"
error via the weak `wfm_stream_sink_*` seam).

The **shared** library is even simpler — `-ldoppler` alone is sufficient.

!!! note "POSIX only"

    `doppler_wfmgen` is built on the same POSIX surface as the `wfmgen` binary
    (the stream sink and `--realtime` pacing), so it is not available on Windows.
