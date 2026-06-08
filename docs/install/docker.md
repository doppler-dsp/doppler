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

`docker-compose.yml` wires up a transmitter and two receivers:

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

| Service       | Description                                                 |
| ------------- | ----------------------------------------------------------- |
| `transmitter` | Generates and publishes IQ samples over ZMQ PUB (port 5555) |
| `receiver-1`  | Subscribes and prints signal stats                          |
| `receiver-2`  | Second subscriber (demonstrates PUB/SUB fan-out)            |
