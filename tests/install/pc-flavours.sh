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
SRC="$(cd "$(dirname "$0")/../.." && pwd)"

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
# matter. Windows has no GNU -rpath, so its relocatable flavour has none.
echo "== the decision, by prefix =="
decide() { # prefix [windows] -> path of the .pc it wrote
    local d
    d="$(mktemp -d)"
    ( cd "$d" && DESTDIR="$d/root" "$CMAKE" \
        -DCMAKE_INSTALL_PREFIX="$1" -DPC_NAME=doppler.pc \
        -DPC_TEMPLATE="$SRC/cmake/doppler.pc.in" -DPC_TMPDIR="$d/tmp" \
        -DPC_LIBDIR=lib -DPC_INCLUDEDIR=include -DPC_TO_PREFIX=../.. \
        -DPC_VERSION=1.2.3 -DPC_FEATURE_CFLAGS= -DPC_WIN32="${2:-}" \
        -P "$SRC/cmake/install_pc.cmake" >/dev/null ) || die "install script failed for $1"
    find "$d" -path '*pkgconfig*' -name doppler.pc | head -n 1
}
expect() { # prefix windows(0|1) flavour(system|reloc) rpath(yes|no)
    local f; f="$(decide "$1" "$([ "$2" = 1 ] && echo 1)")"
    [ -n "$f" ] || die "prefix '$1': nothing written"
    if [ "$3" = system ]; then
        grep -qx 'prefix=/usr' "$f" || die "prefix '$1' should be the system flavour"
    else
        grep -q '^prefix=\${pcfiledir}/' "$f" || die "prefix '$1' should be relocatable"
    fi
    if grep -E '^Libs' "$f" | grep -q -- '-Wl,-rpath'; then got=yes; else got=no; fi
    [ "$got" = "$4" ] || die "prefix '$1' (windows=$2): rpath is '$got', want '$4'"
    say "$1$([ "$2" = 1 ] && echo ' (windows)') -> $3, rpath $4"
}
expect /usr            0 system no
expect /usr/           0 system no
expect /usr/.          0 system no
expect /usr/local      0 reloc   yes
expect /opt/doppler    0 reloc   yes
expect "$HOME/.local"  0 reloc   yes
expect /usr/local      1 reloc   no
expect /usr            1 system  no

echo "pc flavours OK"
