# Docker

## Published container

Every release publishes `ghcr.io/doppler-dsp/doppler:X.Y.Z` (+ `:latest`)
for both `linux/amd64` and `linux/arm64` — the full Python package with the
`cli` and `specan-web` extras pre-installed, so `doppler`, `doppler-fir`,
`doppler-source`, `doppler-specan`, and `wfmgen` are all on `PATH`:

```sh
docker pull ghcr.io/doppler-dsp/doppler:latest
docker run --rm ghcr.io/doppler-dsp/doppler wfmgen --help
docker run --rm ghcr.io/doppler-dsp/doppler python -c "import doppler; print(doppler.__version__)"
```

Pin a specific release instead of `latest`:

```sh
docker run --rm ghcr.io/doppler-dsp/doppler:0.28.1 doppler --help
```

`linux/amd64` installs the exact wheel published to PyPI for that release
(built once, smoke-tested by the release workflow, never rebuilt).
`linux/arm64` has no manylinux wheel to install yet, so that platform
builds from source at image-build time instead — see
[`deploy/docker/Dockerfile.cli`](https://github.com/doppler-dsp/doppler/blob/main/deploy/docker/Dockerfile.cli).

To drive a streaming pipeline from the container, point it at a reachable
`nats-server` (e.g. run one alongside with `docker network` or `--network host`):

```sh
docker run --rm --network host ghcr.io/doppler-dsp/doppler \
    wfmgen --type qpsk --count 4096 --output nats://127.0.0.1:4222/iq
```

## Build the demo image from source

The repository includes a separate, multi-stage root `Dockerfile` and a
`docker-compose.yml` for running the C library, examples, and tests in a
self-contained environment — this is a smaller, C-only image aimed at
local development and the streaming demo below, not the published
container above.

### Build the image

```sh
--8<-- "tests/install/docker-build.sh:build"
```

The image contains:

- `libdoppler.so` installed to `/usr/local/lib`
- C example binaries in `/app/`
- Test binaries (`test_*`) in `/app/`

### Run the tests

```sh
--8<-- "tests/install/docker-build.sh:test"
```

### Docker Compose — full streaming demo

`docker-compose.yml` wires up a transmitter, two receivers, and a spectrum
analyzer over NATS (plus a one-shot `tests` service under the `test`
profile). The streaming services need a reachable `nats-server` (e.g.
`nats-server -js`):

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

Services:

| Service       | Description                                                     |
| ------------- | --------------------------------------------------------------- |
| `transmitter` | Generates and publishes IQ samples over NATS PUB (subject `iq`) |
| `receiver-1`  | Subscribes and prints signal stats                              |
| `receiver-2`  | Second subscriber (demonstrates NATS PUB/SUB fan-out)           |
