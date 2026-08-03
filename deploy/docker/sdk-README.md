# doppler SDK image

You're in `/workspace` with **doppler installed** to `/usr/local` — headers,
the static *and* shared library, the CMake package config and the pkg-config
`.pc`. A downstream `find_package(doppler)` resolves here with no flags. The
dev toolchain (gcc, cmake, make), `uv`, and the pinned `just-makeit` are all
on `PATH`.

Under `examples/` are four real consumers, smallest first:

| Path                      | What it shows                                                                                                                |
| ------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| `examples/consumer/`      | The minimum: `find_package(doppler)` + link `doppler::doppler`.                                                              |
| `examples/standalone/`    | A single-file DSP program linking the static lib.                                                                            |
| `examples/c/`             | The streaming C demos (transmitter / receiver / spectrum).                                                                   |
| `examples/downstream-jm/` | **iqtools** — a full jm project: C core + generated Python bindings, `.pyi`, tests. The flagship "build on doppler" example. |

## Try it

```sh
# 1) The tiniest consumer — links doppler, prints a result.
cd examples/consumer && cmake -B build && cmake --build build && ./build/consumer_shared

# 2) The full jm downstream — builds bindings + runs its whole suite.
cd /workspace/examples/downstream-jm && make test

# 3) Regenerate that project's glue from its manifest, live:
just-makeit apply        # then `make test` again — same result, zero drift.
```

`doppler` is a static-and-shared install, so both `doppler::doppler` and
`doppler::doppler-static` are available; the optional `doppler::stream` NATS
tier is installed too. `pkg-config --cflags --libs doppler` also works.

Verify the install:

```sh
pkg-config --modversion doppler
ls /usr/local/lib/cmake/doppler
```
