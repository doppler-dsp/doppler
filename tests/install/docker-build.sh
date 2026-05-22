#!/usr/bin/env bash
set -euo pipefail

# --8<-- [start:build]
docker build -t doppler .
# --8<-- [end:build]

# --8<-- [start:test]
docker run --rm doppler /app/test_stream
# --8<-- [end:test]

# --8<-- [start:compose]
docker compose up
# --8<-- [end:compose]

# --8<-- [start:compose-down]
docker compose down
# --8<-- [end:compose-down]
