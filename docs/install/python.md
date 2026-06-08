# Python Install

## pip

```sh
--8<-- "tests/install/pip-core.sh:install"
```

!!! success "No system libraries needed"

    The wheel bundles all native dependencies — the streaming extension
    statically links a vendored copy of libzmq. `pip install` works
    out of the box on Linux, macOS, and Windows.

## Verify

```sh
--8<-- "tests/install/pip-core.sh:verify"
```

## Optional extras

| Extra        | Install command                         | Adds                                                             |
| ------------ | --------------------------------------- | ---------------------------------------------------------------- |
| `cli`        | `pip install "doppler-dsp[cli]"`        | `doppler compose` pipeline orchestrator (pydantic, pyyaml, rich) |
| `specan`     | `pip install "doppler-dsp[specan]"`     | Terminal spectrum analyzer (rich)                                |
| `specan-web` | `pip install "doppler-dsp[specan-web]"` | Browser spectrum analyzer (FastAPI + WebSocket)                  |

!!! tip "Install multiple extras at once"

    ```sh
    --8<-- "tests/install/pip-extras.sh:multiple"
    ```
