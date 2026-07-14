# C Quick Start

Everything on this page is executed in CI — the consumer program and all
three build commands are included from the very files the release smoke
runs on linux-x86_64, linux-aarch64, and macos-arm64.

## Get the library

No toolchain needed — `jbx get-doppler` grabs the pre-built release
tarball (headers + `libdoppler.a`/`.so` + the optional stream
component):

```sh
jbx get-doppler                          # extracts to $HOME/.local/doppler
```

Other routes (manual tarball, custom prefixes, system install, building
from source): [Install → C Library](install/c.md).

## The consumer

Any `main.c` works; this is the one CI builds — the FFT example plus one
*optional* streaming call. `dp_pub_*`/`dp_sub_*` live in the optional
`libdoppler_stream`; drop that call and the `_stream` bits below for a
core-only app (then the whole link line is `libdoppler.a -lm`):

```c
--8<-- "tests/install/stream-consumer/app.c:app"
```

## Compile it — three ways

Set the prefix once (wherever `jbx get-doppler` extracted), then pick
any face. All three are built and diffed against each other in CI: the
three binaries must produce identical output.

```sh
PREFIX="$HOME/.local/doppler"
```

=== "cc (static, self-contained)"

    ```sh
    --8<-- "tests/install/stream-consumer/build-three-ways.sh:cc"
    ```

=== "CMake"

    ```cmake
    --8<-- "tests/install/stream-consumer/CMakeLists.txt:cmake"
    ```

    ```sh
    --8<-- "tests/install/stream-consumer/build-three-ways.sh:cmake-commands"
    ```

=== "pkg-config"

    ```sh
    --8<-- "tests/install/stream-consumer/build-three-ways.sh:pkg-config"
    ```

The CMake and pkg-config faces link the shared libraries by default —
add `$PREFIX/lib` to the loader path (`LD_LIBRARY_PATH`, or an rpath) to
run outside the prefix. The `cc` face is static and self-contained.

## Next steps

- [C Examples](examples/c.md) — worked, runnable C programs for every
    major subsystem
- [Streaming Examples](examples/streaming.md) — the NATS PUB/SUB wire
    layer in C and Python
- [C API Reference](c-api/index.md) — every header, generated from the
    Doxygen source
- [Install → C Library](install/c.md) — `find_package` details, static
    vs shared, custom prefixes
