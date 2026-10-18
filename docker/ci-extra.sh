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

# Pinned by value AND by hash: the version a commit names, the bytes the
# release published. Both sums are the release's own SHA256SUMS lines, and
# were re-measured against the downloaded tarballs when they were written.
NATS_VERSION=2.10.22
NATS_SHA256_amd64=db0b3ccbe4cbdd3872ae7486ec4f6b0f85824632a0789f4da2e0a8518390483e
NATS_SHA256_arm64=b4da77b2b194dc5fcf13a1df0dad59ba7e87ae4423f254c50534015b7c8a2369

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

# nats-server as a BINARY, not a container. `make nats-up` shells out to
# `docker run`, which is not available inside a container job -- and the
# stream suite's nats:// tests self-skip when 127.0.0.1:4222 is unreachable,
# so without this the coverage number would quietly drop the whole nats path
# instead of failing. scripts/start-nats.sh prefers this binary when it is on
# PATH and falls back to docker.
#
# curl --fail, to a file, then verified: without -f an error page is saved as
# the tarball and the failure surfaces later as "not in gzip format", naming
# the wrong thing (doppler#1738).
arch="$(dpkg --print-architecture)"
sum_var="NATS_SHA256_$arch"
want="${!sum_var:-}"
[ -n "$want" ] || { echo "no nats-server sha256 pinned for $arch" >&2; exit 1; }
pkg="nats-server-v${NATS_VERSION}-linux-${arch}"
curl -fsSL --retry 5 --retry-all-errors --retry-delay 2 -o /tmp/nats.tar.gz \
    "https://github.com/nats-io/nats-server/releases/download/v${NATS_VERSION}/${pkg}.tar.gz"
echo "$want  /tmp/nats.tar.gz" | sha256sum -c -
tar -xzf /tmp/nats.tar.gz -C /tmp
install -m 755 "/tmp/$pkg/nats-server" /usr/local/bin/nats-server
rm -rf /tmp/nats.tar.gz "/tmp/$pkg"
nats-server --version
