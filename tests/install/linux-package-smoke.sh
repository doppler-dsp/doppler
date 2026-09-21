#!/usr/bin/env bash
# linux-package-smoke.sh — runs INSIDE a distro container (make
# package-linux-smoke): install doppler's .deb or .rpm packages with the
# distro's own package manager, then consume them from /usr.
#
# What it proves, in order, and why each is here:
#   1. the packages INSTALL -- dependencies resolve against the distro's real
#      archive, the -dev/-devel pin on the runtime package is satisfiable;
#   2. the split is right -- after installing ONLY the runtime package there
#      is a versioned libdoppler.so.X.Y and NO dev symlink, header or .a;
#   3. the loader finds it -- `ldconfig -p` lists the soname, which is the
#      trigger / %post doing its job, not the consumer's rpath;
#   4. a consumer builds with no path of ours on its command line --
#      find_package (shared + static) and pkg-config, against /usr;
#   5. /usr/include gained exactly one entry, doppler/ (doppler#1408).
#   6. pkg-config's Cflags are sufficient -- every installed header compiles at
#      a strict -std=c99 from them alone (doppler#1451); meaningful on the
#      glibc 2.28 leg, where the missing feature macro is an error.
#
# Usage: linux-package-smoke.sh <dir holding the built packages>
set -euo pipefail
PKGS="$1"
SRC="$(cd "$(dirname "$0")/../.." && pwd)"
say() { echo "   $*"; }
die() { echo "FAIL: $*" >&2; exit 1; }

if command -v apt-get >/dev/null 2>&1; then
    fmt=deb
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq >/dev/null
    install() { apt-get install -y -qq --no-install-recommends "$@" >/dev/null; }
    toolchain=(gcc libc6-dev cmake make pkg-config)
    rt=("$PKGS"/libdoppler-dsp[0-9]*.deb)
    dev=("$PKGS"/libdoppler-dsp-dev_*.deb)
    tools=("$PKGS"/doppler-dsp-tools_*.deb)
else
    fmt=rpm
    install() { dnf install -y -q "$@" >/dev/null; }
    toolchain=(gcc cmake make pkgconf-pkg-config)
    # The runtime rpm is the one whose name has no -devel and no -tools.
    rt=("$PKGS"/libdoppler-dsp-[0-9]*.rpm)
    dev=("$PKGS"/libdoppler-dsp-devel-*.rpm)
    tools=("$PKGS"/doppler-dsp-tools-*.rpm)
fi
for f in "${rt[0]}" "${dev[0]}" "${tools[0]}"; do
    [ -f "$f" ] || die "no package matching $f — run 'make package-linux'"
done

# ── 1+2: runtime alone ───────────────────────────────────────────────────────
install "${rt[0]}"
# Only the lib roots that EXIST: arm64 Debian has no /usr/lib64 (x86_64 has
# one, for the loader), `find` exits 1 on a missing start point, and under
# pipefail that ended this script in silence -- on a package that had
# installed correctly. Found by the first arm64 run (doppler#1413).
roots=(); for r in /usr/lib /usr/lib64; do [ -d "$r" ] && roots+=("$r"); done
so="$(find "${roots[@]}" -name 'libdoppler.so.*' | head -1)"
[ -n "$so" ] || die "runtime package installed no versioned libdoppler.so.*"
libdir="$(dirname "$so")"
[ ! -e "$libdir/libdoppler.so" ] || die "runtime package ships the dev symlink"
[ ! -e "$libdir/libdoppler.a" ]  || die "runtime package ships the static lib"
[ ! -e /usr/include/doppler ]    || die "runtime package ships headers"
say "runtime only: $(basename "$so"), no dev files ($fmt, $libdir)"

# ── 3: the loader cache ──────────────────────────────────────────────────────
ldconfig -p | grep -q 'libdoppler\.so\.[0-9]' \
    || die "ldconfig -p does not list libdoppler — the trigger/%post did not run"
say "ldconfig lists $(ldconfig -p | grep -o 'libdoppler\.so\.[0-9.]*' | head -1)"

# ── dev + tools ──────────────────────────────────────────────────────────────
install "${dev[0]}" "${tools[0]}"
[ -L "$libdir/libdoppler.so" ] || die "-dev did not install the dev symlink"
[ -f /usr/include/doppler/lo/lo_core.h ] || die "-dev did not install headers"
wfmgen --help >/dev/null || die "wfmgen does not run"
say "dev + tools installed; wfmgen runs"

# ── 5: the namespace ─────────────────────────────────────────────────────────
# What OUR package owns directly under /usr/include, asked of the package
# manager: -dev pulls in libc6-dev, so a before/after listing of the
# directory measures the distro's headers, not doppler's.
if [ "$fmt" = deb ]; then owned() { dpkg -L libdoppler-dsp-dev; }
else owned() { rpm -ql libdoppler-dsp-devel; }; fi
tops="$(owned | sed -n 's#^/usr/include/\([^/]*\).*#\1#p' | sort -u | tr '\n' ' ')"
[ "$tops" = "doppler " ] || die "-dev owns in /usr/include: $tops(want: doppler)"
say "-dev owns only doppler/ under /usr/include"

# ── 4: consume from /usr ─────────────────────────────────────────────────────
install "${toolchain[@]}"
work="$(mktemp -d)"
cmake -S "$SRC/example-projects/consumer" -B "$work/b" \
    -DCMAKE_BUILD_TYPE=Release >"$work/log" 2>&1 \
    && cmake --build "$work/b" >>"$work/log" 2>&1 \
    || { cat "$work/log" >&2; die "find_package consumer did not build"; }
"$work/b/consumer_shared" >/dev/null || die "consumer_shared did not run"
"$work/b/consumer_static" >/dev/null || die "consumer_static did not run"
# shellcheck disable=SC2046
cc "$SRC/example-projects/consumer/main.c" -o "$work/pc" \
    $(pkg-config --cflags --libs doppler) || die "pkg-config consumer"
"$work/pc" >/dev/null || die "pkg-config consumer did not run"
say "find_package (shared + static) and pkg-config consumers build and run"

# ── 6. the .pc's Cflags are SUFFICIENT: every installed header compiles at a
#       strict -std=c99 with nothing but what pkg-config reports (doppler#1451).
# That is the contract a .pc file IS, and it went unasserted: doppler.pc
# lacked the feature-test macro the exported CMake target carried, so 33 of
# 159 headers -- the umbrella doppler.h among them -- did not compile for a
# pkg-config consumer. Asked of the WHOLE installed set, so a new header is
# covered the day it ships, and asked HERE because the answer depends on the
# C library: on glibc >= 2.34 the same compile only warns and this passes
# with the bug live. almalinux:8 (2.28) is the distro in the list that makes
# it a real check.
#
# Two kinds of header are not standalone by design, recognised by what they
# say rather than by name: one whose #error is an INCLUDE-ORDER guard
# ("Include x before y"), and one that needs Python.h, which no C consumer
# has. The pattern is that narrow on purpose: buffer.h and dp_complex.h carry
# a compiler-capability #error, and buffer.h is the header this check exists
# for -- "any #error" skipped it.
inc="$(pkg-config --variable=includedir doppler)/doppler"
n=0; bad=""
while IFS= read -r h; do
    grep -qE '^[[:space:]]*#[[:space:]]*(error[[:space:]]+"Include |include[[:space:]]*<Python\.h>)' \
        "$inc/$h" && continue
    n=$((n + 1))
    printf '#include "%s"\nint main (void) { return 0; }\n' "$h" >"$work/h.c"
    # shellcheck disable=SC2046
    cc -std=c99 -fsyntax-only $(pkg-config --cflags doppler) "$work/h.c" \
        >/dev/null 2>&1 || bad="$bad $h"
done < <(cd "$inc" && find . -name '*.h' | sed 's|^\./||' | sort)
[ "$n" -gt 100 ] || die "only $n installed headers found under $inc -- the walk is broken"
[ -z "$bad" ] || die "not compilable at -std=c99 with pkg-config's own Cflags:$bad"
say "all $n standalone installed headers compile at strict -std=c99 from pkg-config --cflags"
echo "   PASS ($fmt)"
