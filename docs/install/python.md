# Python Install

## pip

```sh
--8<-- "tests/install/pip-core.sh:install"
```

!!! success "No system libraries needed"

    The wheel bundles all native dependencies — the streaming extension
    statically links a vendored copy of `nats.c`. `pip install` works
    out of the box on Linux (x86_64, aarch64), macOS (arm64) and Windows
    (x64), for Python 3.9+.

!!! note "Windows"

    The Windows wheel is the same package as on Linux and macOS, including
    the NATS stream layer (`doppler.stream`, `doppler.wfm.StreamSink`) and
    the `wfmgen` command, which it has carried since
    [#1575](https://github.com/doppler-dsp/doppler/issues/1575).

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
