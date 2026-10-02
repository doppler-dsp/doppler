# syntax=docker/dockerfile:1
#
# The CI toolchain image (HAS_CI_IMAGE). VENDORED from canonical: change it
# there, not here -- standard-check fails on any difference. Built by
# .github/workflows/ci-image.yml, pinned in .github/ci-images.env; why each
# pin exists is scripts/ci-image.py's docstring.
#
# Every input is a build ARG with NO default, refused when empty: a default
# would be a second copy of the pin, and the stale one. The weekly refresh
# picks new values; every other build reads the pinned ones, so a rebuild
# reproduces the pinned package set exactly.
#
# The package list is not here. It is bootstrap.toml's groups
# (CI_IMAGE_GROUPS), installed by a PINNED just-bashit release's
# install-deps.sh -- the same file `make install-deps` reads, so the image
# and a laptop cannot diverge. A project's own extras (a tool no package
# manager has) go in docker/ci-extra.sh, below.

ARG BASE
FROM ${BASE}

ENV DEBIAN_FRONTEND=noninteractive

# Point apt at the snapshot, then PROVE it: fail if any active source names
# anything else, or if none was found (a rewrite that matched nothing would
# leave the live mirror in place and pass). Both layouts (sources.list and
# deb822), both hosts (archive/security and ports): snapshot.ubuntu.com
# serves arm64 from the same /ubuntu tree. The snapshot must not predate the
# base -- the refresh picks both at once, so only a hand-picked pair can,
# and that fails here rather than drifting.
ARG APT_SNAPSHOT
RUN set -eu; \
    case "$APT_SNAPSHOT" in \
      [0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]T[0-9][0-9][0-9][0-9][0-9][0-9]Z) ;; \
      *) echo "APT_SNAPSHOT='$APT_SNAPSHOT' is not YYYYMMDDTHHMMSSZ" >&2; \
         exit 1 ;; \
    esac; \
    snap="https://snapshot.ubuntu.com/ubuntu/${APT_SNAPSHOT}/"; \
    for f in /etc/apt/sources.list /etc/apt/sources.list.d/*.list \
             /etc/apt/sources.list.d/*.sources; do \
        [ -f "$f" ] || continue; \
        sed -i -E \
          "s#https?://(archive|security|ports)\.ubuntu\.com/ubuntu(-ports)?/?#${snap}#g" \
          "$f"; \
    done; \
    srcs="$(cat /etc/apt/sources.list /etc/apt/sources.list.d/*.list \
                /etc/apt/sources.list.d/*.sources 2>/dev/null \
            | grep -E '^(deb |URIs:)' || true)"; \
    [ -n "$srcs" ] || { echo "no apt sources found to pin" >&2; exit 1; }; \
    if printf '%s\n' "$srcs" | grep -vF "$snap"; then \
        echo "^ apt sources not on snapshot $APT_SNAPSHOT" >&2; exit 1; \
    fi; \
    echo "apt pinned to snapshot $APT_SNAPSHOT"

# The bootstrap for the bootstrap: the snapshot is https-only and a stock
# base has no CA bundle, so this ONE apt-get runs without TLS peer
# verification -- to install the bundle. Integrity does not rest on TLS:
# apt verifies every InRelease against the keyring the base ships. Every
# byte installed still comes from the snapshot. git is for actions/checkout
# (without it the action falls back to a history-less tarball); jq for the
# workflow scripts. Every request is bounded, so a stall becomes an error
# the retries cover (just-makeit#1792).
RUN apt-get -o Acquire::https::Verify-Peer=false -o Acquire::Retries=3 \
        -o Acquire::http::Timeout=30 -o Acquire::https::Timeout=30 update \
 && apt-get -o Acquire::https::Verify-Peer=false -o Acquire::Retries=3 \
        -o Acquire::http::Timeout=30 -o Acquire::https::Timeout=30 \
        install -y --no-install-recommends \
        ca-certificates curl git jq make sudo xz-utils \
 && rm -rf /var/lib/apt/lists/*

# The installer, PINNED: a just-bashit release tarball, verified against its
# sha256, its install-deps.sh run in place (it sources its libraries from its
# own directory). Not get-jb.sh + `jbx install-deps`: jbx resolves
# install-deps through aliases.toml at RUN time, so pinning jbx pins nothing
# that installs a package (doppler#1751). Downloaded to a file, then run --
# never piped -- and curl --fail, so an error page is not saved as the file.
ARG JB_VERSION
ARG JB_SHA256
ARG CI_IMAGE_GROUPS
COPY bootstrap.toml /tmp/bootstrap.toml
WORKDIR /tmp
RUN set -eu; \
    [ -n "$JB_VERSION" ] && [ -n "$JB_SHA256" ] \
      || { echo "JB_VERSION and JB_SHA256 are required" >&2; exit 1; }; \
    [ -n "$CI_IMAGE_GROUPS" ] \
      || { echo "CI_IMAGE_GROUPS is required" >&2; exit 1; }; \
    curl -fsSL --retry 5 --retry-all-errors --retry-delay 2 \
      -o /tmp/jb.tar.gz \
      "https://github.com/just-buildit/just-bashit/releases/download/${JB_VERSION}/just-bashit.tar.gz"; \
    echo "${JB_SHA256}  /tmp/jb.tar.gz" | sha256sum -c -; \
    # The tarball's top level IS src/just_bashit/ -- no wrapper directory,
    # so no --strip-components (measured on v0.6.0's asset).
    mkdir -p /tmp/jb && tar -xzf /tmp/jb.tar.gz -C /tmp/jb; \
    for g in $CI_IMAGE_GROUPS; do \
        bash /tmp/jb/src/just_bashit/install-deps.sh -g "$g" /tmp/bootstrap.toml; \
    done; \
    rm -rf /tmp/jb /tmp/jb.tar.gz /tmp/bootstrap.toml /var/lib/apt/lists/*

# The project's extension point, optional: docker/ci-extra.sh installs what
# no package manager has (doppler: nats-server, clang's profile runtime).
# A wildcard matching NOTHING fails a COPY, so bootstrap.toml (always there)
# rides along and the glob may match zero -- the directory is a wildcard too,
# since a missing (or .dockerignored) docker/ fails a literal path. Its `--fingerprint` mode prints
# `tool<TAB>version` lines, hashed below with dpkg's -- a dpkg-only hash
# missed an entire rustup layer once, so anything installed outside dpkg
# must be in it or the weekly comparison is blind to it.
COPY bootstrap.toml docke[r]/ci-extra.s[h] /tmp/ci-extra/
RUN if [ -f /tmp/ci-extra/ci-extra.sh ]; then \
        bash /tmp/ci-extra/ci-extra.sh; \
    fi

# Runs as more than one user: a container job as root, or `--user 1001`
# (the hosted runner's uid, so the mounted workspace stays writable), and the
# local targets as the caller. As root a read-only directory is deletable,
# so a suite gate that relies on it skips (just-makeit#1796) -- the adopter
# picks in its own ci.yml. Tool state cannot live under a HOME an arbitrary
# uid lacks, so caches go to a world-writable dir.
RUN useradd --uid 1001 --create-home ci \
 && mkdir -p /tmp/cache && chmod 1777 /tmp/cache \
 && git config --system --add safe.directory '*'
ENV XDG_CACHE_HOME=/tmp/cache CCACHE_DIR=/tmp/cache/ccache

# The fingerprint: every package and version, plus the extras', hashed. It
# changes only when the CONTENT does -- the question a repin answers.
RUN { dpkg-query -W -f='${Package}\t${Version}\n'; \
      if [ -f /tmp/ci-extra/ci-extra.sh ]; then \
          bash /tmp/ci-extra/ci-extra.sh --fingerprint; \
      fi; \
    } | LC_ALL=C sort > /etc/ci-image-packages \
 && sha256sum /etc/ci-image-packages | cut -d' ' -f1 > /etc/ci-image-fingerprint \
 && rm -rf /tmp/ci-extra \
 && echo "CI image: $(wc -l < /etc/ci-image-packages) packages," \
         "fingerprint $(cat /etc/ci-image-fingerprint)"

WORKDIR /w
