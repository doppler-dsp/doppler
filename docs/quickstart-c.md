# C Quick Start

Get the library, build a small program against it three ways, and run it.
The commands below are the tested ones — copy them as they are.

## Get the library

No toolchain needed — [`jbx`](install/c.md#get-jbx) `get-doppler` grabs
the pre-built release tarball (headers + `libdoppler.a`/`.so` + the optional
stream component):

!!! tip "Don't have `jbx` yet?"

    Install it once, then run the command below:

    ```sh
    . <(curl -sSL https://just-buildit.github.io/get-jb.sh)
    ```

    It lands in `$HOME/.local/bin` and the leading `.` puts that on `PATH`
    for this shell. Details: [Get `jbx`](install/c.md#get-jbx).

```sh
jbx get-doppler                          # extracts to $HOME/.local/doppler
```

Other routes (manual tarball, custom prefixes, system install, building
from source): [Install → C Library](install/c.md).

## The consumer

Any `main.c` works; this is the one the commands below build — the FFT example plus one
*optional* streaming call. `dp_pub_*`/`dp_sub_*` live in the optional
`libdoppler_stream`; drop that call and the `_stream` bits below for a
core-only app (then the whole link line is `libdoppler.a -lm -lpthread`):

```c
--8<-- "tests/install/stream-consumer/app.c:app"
```

## Compile it — three ways

Set the prefix once (wherever [`jbx get-doppler`](install/c.md#get-jbx)
extracted), then pick any face; they build the same program, so use whichever
matches your build system.

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

The `cc` face is static and self-contained. The CMake and pkg-config faces
link the shared libraries in `$PREFIX/lib` and record that directory in the
executable (an rpath), so `./app` runs as built. If you move the binary off
this machine, or delete the prefix, set `LD_LIBRARY_PATH`
(`DYLD_LIBRARY_PATH` on macOS) or re-link.

## Next steps

- [C Examples](examples/c.md) — worked, runnable C programs for every
    major subsystem
- [Streaming Examples](examples/streaming.md) — the NATS PUB/SUB wire
    layer in C and Python
- [C API Reference](c-api/index.md) — every header, generated from the
    Doxygen source
- [Install → C Library](install/c.md) — `find_package` details, static
    vs shared, custom prefixes
