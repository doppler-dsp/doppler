#!/usr/bin/env bash
# ############################################################################
# EXECUTABLE: get-doppler.sh                                                 #
# ############################################################################
# Downloads and extracts the pre-built doppler C library (headers +
# libdoppler.a/.so, no toolchain needed) to an install prefix. A previous
# install at the same prefix is moved aside first, restored automatically if
# the new one fails a sanity check, and restorable on demand with --restore.
#
# Usage:
#   jbx get-doppler [OPTIONS]
#   scripts/get-doppler.sh [OPTIONS]
#
#   -p, --prefix DIR    Install prefix (default: $HOME/.local/doppler).
#   -v, --version X.Y.Z Pin a release (default: latest).
#   -R, --restore        Restore the previous install from its backup and
#                         exit -- no download.
#   -h, --help           Show this message.
#
#   DOPPLER_PREFIX / DOPPLER_VERSION env vars are equivalent to the flags
#   above; a flag, when given, wins over the matching env var.
#
#   No bare-positional PREFIX form: jbx's own calling convention is
#   `jbx SPEC [FUNCTION [ARGS...]]` -- a token right after `get-doppler`
#   would be consumed by jbx itself as a function name to call, never
#   reaching this script's own argument parsing. Always use --prefix.
# ############################################################################
set -euo pipefail

REPO="doppler-dsp/doppler"
PREFIX="${DOPPLER_PREFIX:-$HOME/.local/doppler}"
VERSION="${DOPPLER_VERSION:-}"
RESTORE=0

read -r -d '' HELP <<-'EOF' || true
	Usage: get-doppler.sh [OPTIONS]

	  Downloads and extracts the pre-built doppler C library (headers +
	  libdoppler.a/.so) to an install prefix -- no toolchain needed.

	  -p, --prefix DIR     Install prefix (default: $HOME/.local/doppler).
	  -v, --version X.Y.Z  Pin a release (default: latest).
	  -R, --restore         Restore the previous install from its backup
	                         and exit -- no download.
	  -h, --help            Show this message.

	  DOPPLER_PREFIX / DOPPLER_VERSION env vars work the same as the flags.
	  No bare-positional PREFIX form (jbx's own SPEC/FUNCTION/ARGS calling
	  convention would consume it before this script ever sees it) --
	  always use --prefix.

	  A previous install at PREFIX is moved aside to PREFIX/.get-doppler-backup
	  before a new one is extracted, restored automatically if the new install
	  fails a sanity check, and restorable any time with --restore.
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
	-R | --restore)
		RESTORE=1
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

BACKUP_DIR="$PREFIX/.get-doppler-backup"

# Moves current/, PREFIX's doppler-owned dirs into $1 (a fresh empty dir),
# skipping any that don't exist. Used for both backup and restore.
_move_doppler_dirs() {
	local dest="$1" src="$2"
	for d in include lib bin; do
		[ -e "$src/$d" ] && mv "$src/$d" "$dest/$d"
	done
}

_looks_like_install() {
	[ -f "$1/lib/libdoppler.a" ] && [ -d "$1/include" ]
}

# Refuse a prefix that's a git working tree root and not already a doppler
# install -- extracting include/lib/bin into an unrelated repo (e.g. a
# doppler source checkout at the same path the old default used) pollutes
# it with untracked files instead of erroring cleanly.
if [ -d "$PREFIX/.git" ] && ! _looks_like_install "$PREFIX"; then
	echo "get-doppler: refusing to extract into ${PREFIX} -- it looks like a git repository," >&2
	echo "get-doppler: not a doppler install. Pass a different --prefix." >&2
	exit 1
fi

if [ "$RESTORE" -eq 1 ]; then
	if ! _looks_like_install "$BACKUP_DIR"; then
		echo "get-doppler: no backup found at ${BACKUP_DIR} -- nothing to restore" >&2
		exit 1
	fi
	echo ">> restoring previous install from ${BACKUP_DIR}"
	rm -rf "${PREFIX:?}/include" "${PREFIX:?}/lib" "${PREFIX:?}/bin"
	_move_doppler_dirs "$PREFIX" "$BACKUP_DIR"
	echo "Restored to ${PREFIX}"
	exit 0
fi

OS="$(uname -s)"
ARCH="$(uname -m)"

case "${OS}/${ARCH}" in
Linux/x86_64) PLAT="linux-x86_64" ;;
# aarch64 is the conventional Linux uname -m for this architecture, but
# some environments (certain distros, emulation layers) report arm64
# instead -- same physical architecture, so accept both spellings.
Linux/aarch64 | Linux/arm64) PLAT="linux-aarch64" ;;
Darwin/arm64) PLAT="macos-arm64" ;;
*)
	echo "get-doppler: no pre-built tarball for ${OS}/${ARCH} yet -- available: linux-x86_64, linux-aarch64, macos-arm64" >&2
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

BACKED_UP=0
if _looks_like_install "$PREFIX"; then
	OLD_VERSION=""
	[ -f "$PREFIX/lib/pkgconfig/doppler.pc" ] &&
		OLD_VERSION="$(sed -n 's/^Version: //p' "$PREFIX/lib/pkgconfig/doppler.pc")"
	echo ">> backing up previous install${OLD_VERSION:+ (v${OLD_VERSION})} to ${BACKUP_DIR}"
	rm -rf "${BACKUP_DIR:?}"
	mkdir -p "$BACKUP_DIR"
	_move_doppler_dirs "$BACKUP_DIR" "$PREFIX"
	BACKED_UP=1
fi

mkdir -p "$PREFIX"
tar -xzf "${work}/${TARBALL}" -C "$PREFIX"

if ! _looks_like_install "$PREFIX"; then
	echo "get-doppler: extracted tarball doesn't look like a valid install (missing lib/libdoppler.a) -- something went wrong" >&2
	if [ "$BACKED_UP" -eq 1 ]; then
		echo "get-doppler: rolling back to the previous install" >&2
		rm -rf "${PREFIX:?}/include" "${PREFIX:?}/lib" "${PREFIX:?}/bin"
		_move_doppler_dirs "$PREFIX" "$BACKUP_DIR"
	fi
	exit 1
fi

echo ""
echo "Installed to ${PREFIX}"
if [ "$BACKED_UP" -eq 1 ]; then
	echo "Previous install backed up to ${BACKUP_DIR} -- restore it with:"
	echo "  get-doppler.sh --prefix \"${PREFIX}\" --restore"
fi
echo ""
echo "  CMake:      cmake -B build -DCMAKE_PREFIX_PATH=\"${PREFIX}\""
echo "  pkg-config: export PKG_CONFIG_PATH=\"${PREFIX}/lib/pkgconfig\""
echo "  Plain cc:   cc example.c -I\"${PREFIX}/include\" \"${PREFIX}/lib/libdoppler.a\" -lm -o example"
echo ""
echo "See https://doppler-dsp.github.io/doppler/install/c/ for the full"
echo "find_package / pkg-config integration guide."
