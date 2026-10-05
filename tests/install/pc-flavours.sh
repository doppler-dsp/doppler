#!/usr/bin/env bash
# pc-flavours.sh — the installed pkg-config files come in two flavours, chosen
# at INSTALL time by the prefix (cmake/install_pc.cmake). They differ in how the
# PREFIX is spelled, and in nothing else; neither carries an rpath.
#
#   a relocatable prefix (the tarball, ~/.local)
#       prefix derived from the file's own location, so the tree can move.
#   a system prefix (/usr, which is what CPack stages the .deb/.rpm with)
#       the literal /usr, so pkg-config filters the system -I/-L (doppler#1547).
#
# No rpath, deliberately (see the note on Libs in cmake/doppler.pc.in): the
# libraries find each other through their own $ORIGIN, and a consumer of a prefix
# off the loader path adds one explicit -Wl,-rpath. That recipe is what
# build-three-ways.sh runs, with the loader path cleared.
#
# Usage: pc-flavours.sh BUILD_DIR RELOCATABLE_PREFIX
#   BUILD_DIR            a configured, built tree (it is installed from, twice)
#   RELOCATABLE_PREFIX   an install already made with --prefix <not /usr>
set -euo pipefail

BUILD="${1:?usage: pc-flavours.sh BUILD_DIR RELOCATABLE_PREFIX}"
REL="${2:?usage: pc-flavours.sh BUILD_DIR RELOCATABLE_PREFIX}"
CMAKE="${CMAKE:-cmake}"
SRC="$(cd "$(dirname "$0")/../.." && pwd)"

die() { echo "FAIL: $*" >&2; exit 1; }
say() { echo "   $*"; }

# The libdir is lib, lib64 or lib/<multiarch> by distro: find the file, do not
# guess its directory.
pc_of() { find "$1" -path '*/pkgconfig/doppler.pc' | head -n 1; }
stream_of() { find "$1" -path '*/pkgconfig/doppler_stream.pc' | head -n 1; }

echo "== flavour 1: relocatable prefix =="
pc="$(pc_of "$REL")"
[ -n "$pc" ] || die "no doppler.pc under $REL"
grep -q '^prefix=\${pcfiledir}/' "$pc" \
    || die "relocatable .pc must derive its prefix from its own location: $(grep '^prefix=' "$pc")"
! grep -E '^Libs' "$pc" | grep -q -- 'rpath' \
    || die "doppler.pc must carry no rpath in Libs: the consumer says where the library is"
st="$(stream_of "$REL")"
if [ -n "$st" ]; then
    grep -q '^prefix=\${pcfiledir}/' "$st" || die "doppler_stream.pc is not relocatable"
    ! grep -E '^Libs' "$st" | grep -q -- 'rpath' \
        || die "doppler_stream.pc must carry no rpath in Libs"
fi
say "doppler.pc: prefix from pcfiledir, no rpath"

echo "== flavour 2: system prefix (/usr, as CPack stages the packages) =="
sys="$(mktemp -d)"
trap 'rm -rf "$sys"' EXIT
# DESTDIR stages it, the prefix stays /usr: exactly what CPack does per
# component, and what decides the flavour (DESTDIR must not).
DESTDIR="$sys" "$CMAKE" --install "$BUILD" --prefix /usr --component dev >/dev/null
pc="$(pc_of "$sys")"
[ -n "$pc" ] || die "no doppler.pc staged under $sys"
grep -qx 'prefix=/usr' "$pc" \
    || die "system .pc must name the literal /usr, got: $(grep '^prefix=' "$pc")"
! grep -q 'pcfiledir' "$pc" || die "system .pc still derives its prefix from its location"
! grep -E '^Libs' "$pc" | grep -q -- 'rpath' \
    || die "system .pc must carry no rpath in Libs: ldconfig finds the library"
st="$(stream_of "$sys")"
if [ -n "$st" ]; then
    grep -qx 'prefix=/usr' "$st" || die "doppler_stream.pc must name the literal /usr"
    ! grep -E '^Libs' "$st" | grep -q -- 'rpath' \
        || die "doppler_stream.pc must carry no rpath in Libs"
fi
say "doppler.pc: prefix=/usr, no rpath"

# The point of the literal prefix, asked of pkg-config itself: it must drop the
# system include directory instead of putting it on every command line. Linux
# only: nothing installs under /usr on macOS (SIP), and Homebrew's pkgconf does
# not list /usr/include as a system directory there, so it prints it for any
# .pc however correct -- the text assertions above are the macOS contract.
if [ "$(uname -s)" = Linux ] && command -v pkg-config >/dev/null 2>&1; then
    out="$(PKG_CONFIG_PATH="$(dirname "$pc")" pkg-config --cflags doppler)"
    case " $out " in
        *" -I/usr/include "*) die "pkg-config leaks the system -I/usr/include: $out" ;;
    esac
    libs="$(PKG_CONFIG_PATH="$(dirname "$pc")" pkg-config --libs doppler)"
    case "$libs" in
        *rpath*) die "pkg-config --libs carries an rpath for a system install: $libs" ;;
    esac
    say "pkg-config --cflags: no system -I; --libs: no rpath"
fi
# The decision itself, a table of prefixes run through the install script
# directly (cmake -P): which prefixes are "system", and that spelling does not
# matter.
echo "== the decision, by prefix =="
decide() { # prefix -> path of the .pc it wrote
    local d
    d="$(mktemp -d)"
    ( cd "$d" && DESTDIR="$d/root" "$CMAKE" \
        -DCMAKE_INSTALL_PREFIX="$1" -DPC_NAME=doppler.pc \
        -DPC_TEMPLATE="$SRC/cmake/doppler.pc.in" -DPC_TMPDIR="$d/tmp" \
        -DPC_LIBDIR=lib -DPC_INCLUDEDIR=include -DPC_TO_PREFIX=../.. \
        -DPC_VERSION=1.2.3 -DPC_FEATURE_CFLAGS= \
        -P "$SRC/cmake/install_pc.cmake" >/dev/null ) || die "install script failed for $1"
    find "$d" -path '*pkgconfig*' -name doppler.pc | head -n 1
}
expect() { # prefix flavour(system|reloc)
    local f; f="$(decide "$1")"
    [ -n "$f" ] || die "prefix '$1': nothing written"
    if [ "$2" = system ]; then
        grep -qx 'prefix=/usr' "$f" || die "prefix '$1' should be the system flavour"
    else
        grep -q '^prefix=\${pcfiledir}/' "$f" || die "prefix '$1' should be relocatable"
    fi
    ! grep -E '^Libs' "$f" | grep -q -- 'rpath' || die "prefix '$1': rpath in Libs"
    say "$1 -> $2"
}
expect /usr            system
expect /usr/           system
expect /usr/.          system
expect /usr/local      reloc
expect /opt/doppler    reloc
expect "$HOME/.local"  reloc

echo "pc flavours OK"
