#!/usr/bin/env bash
# docs-walk-build.sh -- BUILD the candidate artifacts for the docs walk, inside
# a container of the walk's own image (so the glibc matches). Candidate only:
# the walked containers stay bare, so a tool the pages forget to install is a
# failure there; the toolchain this needs (uv, patchelf, dpkg-dev ...) lives
# only in THIS container, which is thrown away.
#
#   docs-walk-build.sh <repo, read-only> <out dir> <version>
#
# Builds what is missing from <out dir>: the wheel, the C tarball, the .debs,
# and a tarball of the tree (`src.tar.gz`, prefixed `doppler-<version>/`, which
# stands in for the release archive the consumer page downloads). Anything
# already in <out dir> (CI's own artifacts) is used as given.
set -euo pipefail
SRC="${1:?repo}"; OUT="${2:?out dir}"; V="${3:?version}"
: "${UV_VERSION:?make docs-walk passes UV_VERSION}"
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq >/dev/null
apt-get install -y -qq build-essential cmake git python3 python3-venv \
    python3-pip python3-dev patchelf dpkg-dev file curl ca-certificates >/dev/null
git config --global --add safe.directory '*'
work="$(mktemp -d)"
git clone -q "$SRC" "$work/src"
have() { ls "$OUT"/$1 >/dev/null 2>&1; }

have 'doppler_dsp-*.whl' || {
    python3 -m venv "$work/venv"
    "$work/venv/bin/pip" install -q "uv==$UV_VERSION"
    PATH="$work/venv/bin:$PATH" "$work/venv/bin/pip" wheel -q --no-deps \
        "$work/src" -w "$OUT"
}
have 'doppler-*linux*.tar.gz' || {
    make -C "$work/src" package-c-tarball VERSION="$V" >/dev/null
    cp "$work"/src/dist/doppler-*linux*.tar.gz "$OUT"/
}
have '*.deb' || {
    make -C "$work/src" package-deb >/dev/null
    cp "$work"/src/dist/linux-packages/*.deb "$OUT"/
}
have src.tar.gz || git -C "$work/src" archive --prefix="doppler-$V/" HEAD | gzip > "$OUT/src.tar.gz"
ls -l "$OUT"
