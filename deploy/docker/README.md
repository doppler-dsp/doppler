# doppler container images

One image per thing you'd actually do with doppler — **use** it, **build on**
it, or **deploy** its streaming services. Every docker build is driven through
the Makefile (the single driver — never a raw `docker build`):

| Image                         | For                                            | Dockerfile                                         | Built by                           | Published                                   |
| ----------------------------- | ---------------------------------------------- | -------------------------------------------------- | ---------------------------------- | ------------------------------------------- |
| **`doppler`**                 | *using* doppler — CLIs + ~70 demos             | `Dockerfile.cli`                                   | release                            | `ghcr.io/doppler-dsp/doppler`               |
| **`doppler-sdk`**             | *building on* doppler — your own C/jm project  | `Dockerfile.examples` (`--target sdk`)             | `make docker-sdk`                  | `ghcr.io/doppler-dsp/doppler-sdk`           |
| **`doppler-downstream-jm`**   | the flagship worked example, pre-built + green | `Dockerfile.examples` (`--target downstream-jm`)   | release / `make docker-downstream` | `ghcr.io/doppler-dsp/doppler-downstream-jm` |
| **`doppler-stream-services`** | the `docker-compose` streaming pipeline        | `Dockerfile.examples` (`--target stream-services`) | `make docker-stream`               | local (via compose)                         |
| **`stream_tool`**             | the k8s produce/consume deploy binary          | `Dockerfile`                                       | `deploy/` k8s flow                 | in-cluster                                  |

`make docker-examples` builds + smokes all three build-on-doppler images.

## The one image that is not an image *of* doppler

| Image                  | For                                    | Dockerfile            | Built by          | Published          |
| ---------------------- | -------------------------------------- | --------------------- | ----------------- | ------------------ |
| **`doppler-glibc228`** | *gating* the glibc floor — a toolchain | `Dockerfile.glibc228` | `make glibc-gate` | never (local + CI) |

Every image above bakes doppler *in*. This one bakes nothing in: it is Debian
10 (glibc 2.28) plus a build toolchain, and `make glibc-gate` bind-mounts the
checkout into it, builds out-of-tree into `build-glibc228/`, smoke-runs the C
examples, and points the existing `glibc-check` at the result.

It exists because `glibc-check` is pure inspection — it only means anything
against a build *made* on the floor. Until this image, that build could only
happen inside CI's job, so the answer arrived one push at a time. The apt and
cmake bootstrap that used to sit inline in `ci.yml` lives in the Dockerfile
now, and the job is a single `make glibc-gate` — the same command a laptop
runs.

## Why one Dockerfile for three images

`Dockerfile.examples` compiles and *installs* this checkout's doppler **once**
in a shared `doppler-build` stage; every image copies from it, so all three are
version-locked to the same source — a downstream `find_package(doppler)`
resolves the very library built beside it, never a drifting pin.

```mermaid
flowchart TD
    SRC["this checkout"] --> BUILD["<b>doppler-build</b><br/>cmake --install → /opt/doppler<br/>+ examples/c binaries"]
    BUILD --> DEV["<b>dev-base</b><br/>toolchain · uv · just-makeit<br/>doppler C SDK in /usr/local"]
    DEV --> SDK["<b>sdk</b><br/>+ all example sources<br/>→ /workspace"]
    DEV --> DJM["<b>downstream-jm</b><br/>iqtools built &amp; tested<br/>→ /iqtools"]
    BUILD --> STR["<b>stream-services</b><br/>static streaming binaries<br/>on a slim runtime"]
```

- **sdk** / **downstream-jm** consume the *installed* doppler (the C SDK) and
    the doppler *Python* package from PyPI — the real downstream pattern (link the
    SDK, import the API). The C SDK is version-locked from source; the Python
    package floats at latest (a test-helper dependency).
- **stream-services** lifts the statically-linked `examples/c` streaming
    binaries straight out of the builder onto a slim base — no toolchain, no
    Python, ~116 MB.

## Files

| File                  | Role                                                 |
| --------------------- | ---------------------------------------------------- |
| `Dockerfile.cli`      | runtime "try it" image (wheel + demos)               |
| `Dockerfile.examples` | the shared-base multi-target build-on-doppler images |
| `Dockerfile`          | k8s `stream_tool` (produce/consume)                  |
| `Dockerfile.glibc228` | Debian 10 build toolchain for the `glibc-gate` gate  |
| `stream_tool.c`       | its source                                           |
| `*-README.md`         | the in-image guide COPYed into each image            |

See [`docs/install/docker.md`](../../docs/install/docker.md) for the user-facing
walkthrough.
