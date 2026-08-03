#!/usr/bin/env bash
# Smoke-test a built doppler container image — ONE definition of each image's
# check, shared by `make docker-*` (local `:dev` images) and release.yml's
# publish-container job (pushed multi-arch ghcr images, invoked once per
# --platform). Keeping the command here is what stops the CI smoke and the
# published-image smoke from drifting apart (they had: the release SDK check
# silently degraded to a version print, and the downstream-jm check carried a
# shell-quoting SyntaxError — both because each was re-typed by hand).
#
# The arch label is printed by the shell HEADER, never interpolated into the
# in-container command — so the smoke command stays byte-identical everywhere
# and there is no quoting to get wrong.
#
# Usage: smoke-image.sh <kind> <image-ref> [platform]
#   kind       runtime | sdk | downstream | stream
#   image-ref  e.g. doppler-sdk:dev  or  ghcr.io/doppler-dsp/doppler-sdk:1.2.3
#   platform   optional `docker --platform` value (e.g. linux/arm64);
#              omitted → the host architecture
set -euo pipefail

kind="${1:?usage: smoke-image.sh <kind> <image-ref> [platform]}"
image="${2:?usage: smoke-image.sh <kind> <image-ref> [platform]}"
platform="${3:-}"

# docker run wrapper: add --platform only when one was requested.
run() {
  if [ -n "${platform}" ]; then
    docker run --rm --platform "${platform}" "${image}" "$@"
  else
    docker run --rm "${image}" "$@"
  fi
}

printf '==> smoke: %s  %s%s\n' "${kind}" "${image}" \
  "${platform:+  (${platform})}"

case "${kind}" in
runtime)
  run python -c "import doppler; print('doppler', doppler.__version__)"
  ;;
sdk)
  # The install is real only if a downstream can find + link it: build the
  # smallest consumer against it, from scratch, and run the result.
  run bash -c 'pkg-config --modversion doppler && \
     cd examples/consumer && cmake -B build >/dev/null && \
     cmake --build build >/dev/null && ./build/consumer_shared'
  ;;
downstream)
  run python3 -c "from iqtools.capture import Capture, RawCapture; \
                  print('iqtools ok:', Capture.__name__, RawCapture.__name__)"
  ;;
stream)
  run sh -c 'command -v transmitter && command -v receiver && \
     command -v spectrum_analyzer'
  ;;
*)
  echo "smoke-image.sh: unknown kind '${kind}'" >&2
  echo "  expected: runtime | sdk | downstream | stream" >&2
  exit 2
  ;;
esac
