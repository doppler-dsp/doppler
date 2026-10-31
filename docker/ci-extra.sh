#!/usr/bin/env bash
# docker/ci-extra.sh -- doppler's extras for the shared CI image.
#
# The image itself is canonical's (HAS_CI_IMAGE): docker/ci.Dockerfile is
# vendored, installs bootstrap.toml's CI_IMAGE_GROUPS through a pinned
# just-bashit release, and runs this file as root once those are in. It holds
# only what no package list can say, and it is one of the image's SOURCES --
# editing it moves CI_IMAGE_SOURCE_HASH, so `make ci-image-check` (on lint)
# refuses the tree until a push of the branch rebuilds and repins the image.
#
#   ci-extra.sh                install the extras (the Dockerfile's RUN)
#   ci-extra.sh --fingerprint  print `tool<TAB>version` lines, hashed with
#                              dpkg's into CI_IMAGE_FINGERPRINT_<key>
set -euo pipefail

# nats-server is NOT installed here any more: it is bootstrap.toml's `nats`
# group, in CI_IMAGE_GROUPS, so the image and `make install-deps` read one pin
# (#2712). It is still FINGERPRINTED here, below, because it lives outside
# dpkg and a dpkg-only hash would not see it change.

# Every tool installed outside dpkg, so the weekly comparison can see it: a
# dpkg-only hash once left an entire rustup layer invisible. These are the
# lines doppler's own Dockerfile.ci hashed, VERBATIM -- cargo and rustc are
# apt's, but their versions ARE the Rust floor gh-887 pinned, so they stay.
if [ "${1:-}" = --fingerprint ]; then
    printf 'rustup\t%s\n' "$(rustc --version)"
    printf 'cargo\t%s\n'  "$(cargo --version)"
    printf 'nats-server\t%s\n' "$(nats-server --version)"
    exit 0
fi

# clang's profile runtime is installed by bootstrap.toml's `clang-rt` group,
# which is in CI_IMAGE_GROUPS: one declaration serves the image and a dev
# box's `make install-deps`. That group tolerates a missing package on a
# release whose clang bundles the runtime (22.04), so the link below is the
# assertion.
# It is the same probe `make coverage`'s preflight runs, so an image that
# cannot link it is never published, instead of failing the coverage job
# later with a message about a missing package.
printf 'int main(void){return 0;}\n' > /tmp/probe.c
clang -fprofile-instr-generate -fcoverage-mapping /tmp/probe.c -o /tmp/probe
rm -f /tmp/probe.c /tmp/probe

# The nats group installed the broker before this file ran; asserting it here
# means an image whose install-deps skipped or lost it is never published.
# Without it every nats:// test SELF-SKIPS, which reads as green.
nats-server --version
