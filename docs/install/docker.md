# Docker

The repository includes a multi-stage `Dockerfile` and a
`docker-compose.yml` for running the library, examples, and tests in a
self-contained environment.

## Build the image

```sh
--8<-- "tests/install/docker-build.sh:build"
```

The image contains:

- `libdoppler.so` installed to `/usr/local/lib`
- C example binaries in `/app/`
- Test binaries (`test_*`) in `/app/`

## Run the tests

```sh
--8<-- "tests/install/docker-build.sh:test"
```

## Docker Compose — full streaming demo

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

| Service       | Description                                                  |
| ------------- | -------------------------------------------------------------- |
| `transmitter` | Generates and publishes IQ samples over NATS PUB (subject `iq`) |
| `receiver-1`  | Subscribes and prints signal stats                              |
| `receiver-2`  | Second subscriber (demonstrates NATS PUB/SUB fan-out)           |
