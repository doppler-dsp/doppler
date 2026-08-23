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
  # An import is not what this image is FOR. Its whole pitch is the ~70
  # demos under /examples, and the docs' first command runs one of them --
  # which is how gh-954 shipped: awgn_demo.py wrote to a repo-relative
  # docs/assets/ that does not exist in the container, so the published
  # image failed the very line the install page tells a reader to type,
  # while this smoke went green on an import.
  #
  # The demo is READ OUT of the doc snippet rather than named again here,
  # so the smoke cannot check one command while the page advertises
  # another. `${0}` locates the checkout, which release.yml also has.
  demo=$(sed -n \
    's|^docker run .* python \([A-Za-z0-9_]*\.py\).*|\1|p' \
    "$(dirname "${0}")/../tests/install/docker-build.sh" | head -n1)
  if [ -z "${demo}" ]; then
    echo "smoke-image.sh: no 'python <demo>.py' line in the runtime" \
         "doc snippet -- the smoke has nothing to check" >&2
    exit 2
  fi
  echo "==> runtime: running the documented demo (${demo})"
  run python "${demo}"
  ;;
sdk)
  # The install is real only if a downstream can find + link it: build the
  # smallest consumer against it, from scratch, and run the result.
  run bash -c 'pkg-config --modversion doppler && \
     cd example-projects/consumer && cmake -B build >/dev/null && \
     cmake --build build >/dev/null && ./build/consumer_shared'
  ;;
downstream)
  run python3 -c "from iqtools.capture import Capture, RawCapture; \
                  print('iqtools ok:', Capture.__name__, RawCapture.__name__)"
  ;;
stream)
# EXECUTE each binary, do not merely locate it. This was `command -v` on all
# three, which passes on a binary that can never run: the loader is not
# involved in resolving a name on PATH. That is exactly what happened — the
# binaries were compiled on glibc 2.39 and shipped onto a 2.36 runtime, so
# every one of them died at startup with
#
#   transmitter: libm.so.6: version `GLIBC_2.38' not found
#
# while this smoke, and therefore CI's Docker job, stayed green. `--help` is
# the cheapest thing that forces a real exec: the dynamic linker resolves
# every DT_NEEDED and every versioned symbol before main() is entered, so an
# ABI mismatch fails here even though nothing connects to a broker.
  run sh -c 'transmitter --help >/dev/null && receiver --help >/dev/null && \
     spectrum_analyzer --help >/dev/null && \
     echo "stream: all three binaries exec cleanly"'
  ;;
*)
  echo "smoke-image.sh: unknown kind '${kind}'" >&2
  echo "  expected: runtime | sdk | downstream | stream" >&2
  exit 2
  ;;
esac
