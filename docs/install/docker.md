# Docker

doppler ships as a small family of purpose-built images — one per thing you'd
actually do. Pick by intent:

| Image                                                          | You want to…                                  | Published                                 |
| -------------------------------------------------------------- | --------------------------------------------- | ----------------------------------------- |
| [`doppler`](#runtime-try-it)                                   | **use** doppler — run the CLIs and the demos  | `ghcr.io/doppler-dsp/doppler`             |
| [`doppler-sdk`](#sdk-build-on-doppler)                         | **build on** doppler — your own C/jm project  | `ghcr.io/doppler-dsp/doppler-sdk`         |
| [`doppler-downstream-jm`](#showcase-a-full-downstream-project) | see a **complete worked** downstream, running | built locally by `make docker-downstream` |
| [compose services](#streaming-demo-docker-compose)             | run the **streaming** pipeline                | built locally by `docker compose`         |

All published images are multi-arch (`linux/amd64` + `linux/arm64`) and tagged
`:X.Y.Z` per release plus `:latest`.

## Runtime — try it

The published pull-and-run image: the full Python package with the `cli` and
`specan-web` extras, numpy/scipy/matplotlib, and the ~70 example scripts under
`/examples`. `doppler`, `doppler-fir`, `doppler-source`, `doppler-specan`, and
`wfmgen` are all on `PATH`.

```sh
--8<-- "tests/install/docker-build.sh:runtime"
```

Every example asserts its own physics — exit 0 means it ran *and* checked out.
Both platforms install the exact wheel published to PyPI for that release
(never rebuilt) — see
[`deploy/docker/Dockerfile.cli`](https://github.com/doppler-dsp/doppler/blob/main/deploy/docker/Dockerfile.cli).

Pin a release instead of `latest`, or drive a streaming pipeline at a reachable
`nats-server`:

```sh
docker run --rm ghcr.io/doppler-dsp/doppler:0.40.0 doppler --help
docker run --rm --network host ghcr.io/doppler-dsp/doppler \
    wfmgen --type qpsk --count 4096 --output nats://127.0.0.1:4222/iq
```

## SDK — build on doppler

doppler installed to `/usr/local` (headers, static *and* shared library, CMake
package config, pkg-config `.pc`) plus the dev toolchain, `uv`, and the pinned
`just-makeit`. A downstream `find_package(doppler)` resolves with no flags.
Under `/workspace/examples` are four real consumers, smallest to largest —
`consumer/`, `standalone/`, `c/`, and the full jm project `downstream-jm/`.

```sh
--8<-- "tests/install/docker-build.sh:sdk"
```

Then, inside:

```sh
cd examples/consumer && cmake -B build && cmake --build build && ./build/consumer_shared
```

## Showcase — a full downstream project

`doppler-downstream-jm` is **iqtools** — a real IQ-capture reader, a C library
*and* a generated Python package, built on doppler in ~140 lines of C plus a few
manifest tables. Building the image runs iqtools' whole suite; you land in
`/iqtools` ready to explore and re-run the jm codegen loop. It's built from the
checkout rather than published — the build itself is the showcase:

```sh
--8<-- "tests/install/docker-build.sh:downstream"
```

```sh
just-makeit apply && make test    # regenerate the glue, rebuild, still green
```

## Build any image from source

The build-on-doppler images are driven through the Makefile — the single driver
for every docker build here:

```sh
--8<-- "tests/install/docker-build.sh:build-local"
```

## Streaming demo — docker compose

`docker-compose.yml` wires a transmitter, two receivers, and a spectrum
analyzer over NATS. Compose builds one lean image (the `stream-services` target
of [`deploy/docker/Dockerfile.examples`](https://github.com/doppler-dsp/doppler/blob/main/deploy/docker/Dockerfile.examples),
carrying just the statically-linked streaming binaries) and runs all four
services on it against a bundled `nats-server`:

```sh
--8<-- "tests/install/docker-build.sh:compose"
```

!!! warning "Foreground process"

    `docker compose up` runs in the foreground and streams all service
    logs to the terminal. Use `docker compose up -d` to detach, then
    `docker compose logs -f` to follow logs separately.

```sh
--8<-- "tests/install/docker-build.sh:compose-down"
```

| Service             | Description                                                     |
| ------------------- | --------------------------------------------------------------- |
| `transmitter`       | Generates and publishes IQ samples over NATS PUB (subject `iq`) |
| `receiver-1`        | Subscribes and prints signal stats                              |
| `receiver-2`        | Second subscriber (demonstrates NATS PUB/SUB fan-out)           |
| `spectrum-analyzer` | ASCII spectrum display                                          |
