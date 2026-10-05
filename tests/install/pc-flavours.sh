#!/usr/bin/env bash
# pc-flavours.sh — the installed pkg-config files come in two flavours, chosen
# at INSTALL time by the prefix (cmake/install_pc.cmake), and each side has its
# own contract:
#
#   a relocatable prefix (the tarball, ~/.local)
#       prefix derived from the file's own location, so the tree can move, and
#       an rpath in Libs, so what a consumer links it can run. (The run itself
#       is build-three-ways.sh's job, with the loader path cleared.)
#   a system prefix (/usr, which is what CPack stages the .deb/.rpm with)
#       the literal /usr, so pkg-config filters the system -I/-L (doppler#1547),
#       and NO rpath: ldconfig, not the binary, tells the loader where it is.
#
# Usage: pc-flavours.sh BUILD_DIR RELOCATABLE_PREFIX
#   BUILD_DIR            a configured, built tree (it is installed from, twice)
#   RELOCATABLE_PREFIX   an install already made with --prefix <not /usr>
set -euo pipefail

BUILD="${1:?usage: pc-flavours.sh BUILD_DIR RELOCATABLE_PREFIX}"
REL="${2:?usage: pc-flavours.sh BUILD_DIR RELOCATABLE_PREFIX}"
CMAKE="${CMAKE:-cmake}"

die() { echo "FAIL: $*" >&2; exit 1; }
say() { echo "   $*"; }

# The libdir is lib, lib64 or lib/<multiarch> by distro: find the file, do not
# guess its directory.
pc_of() { find "$1" -path '*/pkgconfig/doppler.pc' | head -n 1; }
stream_of() { find "$1" -path '*/pkgconfig/doppler_stream.pc' | head -n 1; }

# GNU -rpath does not exist on the COFF toolchains Windows builds with; the
# relocatable flavour leaves it out there on purpose.
case "$(uname -s)" in MINGW* | MSYS* | CYGWIN*) windows=1 ;; *) windows=0 ;; esac

echo "== flavour 1: relocatable prefix =="
pc="$(pc_of "$REL")"
[ -n "$pc" ] || die "no doppler.pc under $REL"
grep -q '^prefix=\${pcfiledir}/' "$pc" \
    || die "relocatable .pc must derive its prefix from its own location: $(grep '^prefix=' "$pc")"
if [ "$windows" = 0 ]; then
    grep -E '^Libs' "$pc" | grep -q -- '-Wl,-rpath,\${libdir}' \
        || die "relocatable .pc must carry an rpath, or what it links cannot run"
fi
st="$(stream_of "$REL")"
if [ -n "$st" ]; then
    grep -q '^prefix=\${pcfiledir}/' "$st" || die "doppler_stream.pc is not relocatable"
    grep -q '^Requires: doppler$' "$st" \
        || die "doppler_stream.pc must Require doppler: that is how it gets the rpath"
fi
say "doppler.pc: prefix from pcfiledir$([ "$windows" = 0 ] && echo ', rpath in Libs')"

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
# system include directory instead of putting it on every command line.
if command -v pkg-config >/dev/null 2>&1; then
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
echo "pc flavours OK"
