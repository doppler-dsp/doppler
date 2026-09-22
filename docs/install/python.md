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

!!! note "What the Windows wheel leaves out"

    The NATS stream layer is not ported to Windows yet, so the Windows
    wheel has no `doppler.stream`, no `doppler.wfm.StreamSink` and no
    `wfmgen` command (it prints that it is not available on this platform).
    Everything else, including `doppler.wfm.Composer`, `Writer` and
    `Reader`, is the same as on Linux and macOS. Tracked on
    [#1364](https://github.com/doppler-dsp/doppler/issues/1364).

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
