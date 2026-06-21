#!/usr/bin/env bash
#
# Build and run the Python 3.9 end-to-end validation of the *published*
# doppler-dsp wheel (no build toolchain in the image). Surfaces the PASS/FAIL
# table and the container's exit code.
#
#   bash deploy/validation/run-py39-e2e.sh
#
# Override the version under test with DOPPLER_VERSION (must have a cp39 wheel):
#   DOPPLER_VERSION=0.17.0 bash deploy/validation/run-py39-e2e.sh
#
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
image="doppler-wfm-py39-e2e"
version="${DOPPLER_VERSION:-0.17.0}"

echo ">> building ${image} (doppler-dsp==${version}, python:3.9-slim)"
docker build \
  --build-arg "DOPPLER_VERSION=${version}" \
  -f "${here}/Dockerfile.py39-e2e" \
  -t "${image}" \
  "${here}"

echo ">> running e2e"
docker run --rm "${image}"
