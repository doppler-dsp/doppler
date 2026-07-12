#!/usr/bin/env bash
# ############################################################################
# EXECUTABLE: get-doppler.sh                                                 #
# ############################################################################
# Downloads and extracts the pre-built doppler C library (headers +
# libdoppler.a/.so, no toolchain needed) to an install prefix.
#
# Usage:
#   jbx get-doppler [OPTIONS]
#   scripts/get-doppler.sh [OPTIONS]
#
#   -p, --prefix DIR    Install prefix (default: $HOME/doppler).
#   -v, --version X.Y.Z Pin a release (default: latest).
#   -h, --help           Show this message.
#
#   DOPPLER_PREFIX / DOPPLER_VERSION env vars are equivalent to the flags
#   above; a flag, when given, wins over the matching env var.
# ############################################################################
set -euo pipefail

REPO="doppler-dsp/doppler"
PREFIX="${DOPPLER_PREFIX:-$HOME/doppler}"
VERSION="${DOPPLER_VERSION:-}"

read -r -d '' HELP <<-'EOF' || true
	Usage: get-doppler.sh [OPTIONS]

	  Downloads and extracts the pre-built doppler C library (headers +
	  libdoppler.a/.so) to an install prefix -- no toolchain needed.

	  -p, --prefix DIR     Install prefix (default: $HOME/doppler).
	  -v, --version X.Y.Z  Pin a release (default: latest).
	  -h, --help            Show this message.

	  DOPPLER_PREFIX / DOPPLER_VERSION env vars work the same as the flags.
EOF

while [ $# -gt 0 ]; do
	case "$1" in
	-p | --prefix)
		PREFIX="$2"
		shift 2
		;;
	--prefix=*)
		PREFIX="${1#*=}"
		shift
		;;
	-v | --version)
		VERSION="$2"
		shift 2
		;;
	--version=*)
		VERSION="${1#*=}"
		shift
		;;
	-h | --help)
		printf '%s\n' "$HELP"
		exit 0
		;;
	*)
		echo "get-doppler: unknown option: $1" >&2
		printf '%s\n' "$HELP" >&2
		exit 1
		;;
	esac
done

OS="$(uname -s)"
ARCH="$(uname -m)"

case "${OS}/${ARCH}" in
Linux/x86_64) PLAT="linux-x86_64" ;;
Darwin/arm64) PLAT="macos-arm64" ;;
*)
	echo "get-doppler: no pre-built tarball for ${OS}/${ARCH} yet -- available: linux-x86_64, macos-arm64" >&2
	echo "get-doppler: build from source instead (see quickstart.md#build-from-source), or if you only need" >&2
	echo "get-doppler: the Python bindings, 'pip install doppler-dsp' ships wheels for more platforms" >&2
	exit 1
	;;
esac

if [ -z "$VERSION" ]; then
	VERSION="$(curl -fsSL "https://api.github.com/repos/${REPO}/releases/latest" |
		sed -n 's/.*"tag_name": *"v\([^"]*\)".*/\1/p')"
	if [ -z "$VERSION" ]; then
		echo "get-doppler: could not resolve the latest release (GitHub API unreachable or rate-limited) -- pass --version X.Y.Z instead" >&2
		exit 1
	fi
fi

TARBALL="doppler-${VERSION}-${PLAT}.tar.gz"
URL="https://github.com/${REPO}/releases/download/v${VERSION}/${TARBALL}"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

echo ">> doppler v${VERSION} (${PLAT}) -> ${PREFIX}"
if ! curl -fsSL -o "${work}/${TARBALL}" "$URL"; then
	echo "get-doppler: download failed: $URL" >&2
	echo "get-doppler: does v${VERSION} exist for ${PLAT}? see https://github.com/${REPO}/releases" >&2
	exit 1
fi

mkdir -p "$PREFIX"
tar -xzf "${work}/${TARBALL}" -C "$PREFIX"

echo ""
echo "Installed to ${PREFIX}"
echo ""
echo "  CMake:      cmake -B build -DCMAKE_PREFIX_PATH=\"${PREFIX}\""
echo "  pkg-config: export PKG_CONFIG_PATH=\"${PREFIX}/lib/pkgconfig\""
echo "  Plain cc:   cc example.c -I\"${PREFIX}/include\" \"${PREFIX}/lib/libdoppler.a\" -lm -o example"
echo ""
echo "See https://doppler-dsp.github.io/doppler/install/c/ for the full"
echo "find_package / pkg-config integration guide."
