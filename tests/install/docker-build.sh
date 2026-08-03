#!/usr/bin/env bash
set -euo pipefail

# Snippet source for docs/install/docker.md. Each region is one runnable
# command block. The build-on-doppler images are driven through the Makefile,
# which is the single driver for every docker build (never a raw
# `docker build`).

# --8<-- [start:runtime]
# The runtime "try it" image — doppler installed, ~70 demos under /examples.
docker run --rm ghcr.io/doppler-dsp/doppler:latest python awgn_demo.py
docker run --rm -it ghcr.io/doppler-dsp/doppler:latest   # shell among the demos
# --8<-- [end:runtime]

# --8<-- [start:sdk]
# The SDK image — build your OWN C/jm project on doppler. Pull the published
# image, or build it locally with `make docker-sdk`.
docker run --rm -it ghcr.io/doppler-dsp/doppler-sdk:latest
# --8<-- [end:sdk]

# --8<-- [start:downstream]
# The iqtools showcase — a full downstream project, shipped pre-built and green.
docker run --rm -it ghcr.io/doppler-dsp/doppler-downstream-jm:latest
# --8<-- [end:downstream]

# --8<-- [start:build-local]
# Build any image from this checkout through the Makefile:
make docker-sdk          # doppler-sdk:dev
make docker-downstream   # doppler-downstream-jm:dev
make docker-stream       # doppler-stream-services:dev (used by compose)
# --8<-- [end:build-local]

# --8<-- [start:compose]
docker compose up
# --8<-- [end:compose]

# --8<-- [start:compose-down]
docker compose down
# --8<-- [end:compose-down]
