#!/usr/bin/env bash
# test_install.sh — verify that the installed pkg-config and CMake config work.
#
# Run after: sudo cmake --install build  (or: sudo make install)
#
# Usage:
#   bash c/tests/test_install.sh [install-prefix]
#
# The install prefix defaults to /usr/local.  Override for non-root installs:
#   bash c/tests/test_install.sh "$HOME/.local"

set -euo pipefail

PREFIX="${1:-/usr/local}"
PASS=0
FAIL=0

ok()   { echo "  PASS: $*"; PASS=$((PASS+1)); }
fail() { echo "  FAIL: $*"; FAIL=$((FAIL+1)); }

echo ""
echo "========================================"
echo "doppler install verification"
echo "  prefix: $PREFIX"
echo "========================================"

# pkg-config search path: cover standard prefix, GNUInstallDirs multiarch path,
# and the Debian/Ubuntu multiarch triplet directory.
MULTIARCH=$(gcc -dumpmachine 2>/dev/null || true)
export PKG_CONFIG_PATH="\
${PREFIX}/lib/pkgconfig:\
${PREFIX}/lib/${MULTIARCH}/pkgconfig:\
${PREFIX}/share/pkgconfig:\
${PKG_CONFIG_PATH:-}"

# ----------------------------------------------------------------
# 1. pkg-config
# ----------------------------------------------------------------
echo ""
echo "--- pkg-config ---"

if pkg-config --exists doppler 2>/dev/null; then
    ok "pkg-config --exists doppler"
else
    fail "pkg-config --exists doppler (PKG_CONFIG_PATH=${PKG_CONFIG_PATH})"
fi

if VERSION=$(pkg-config --modversion doppler 2>/dev/null); then
    ok "pkg-config --modversion doppler => ${VERSION}"
else
    fail "pkg-config --modversion doppler"
fi

if CFLAGS=$(pkg-config --define-variable=prefix="${PREFIX}" --cflags doppler 2>/dev/null); then
    ok "pkg-config --cflags => ${CFLAGS}"
else
    fail "pkg-config --cflags doppler"
fi

if LIBS=$(pkg-config --define-variable=prefix="${PREFIX}" --libs doppler 2>/dev/null); then
    ok "pkg-config --libs => ${LIBS}"
else
    fail "pkg-config --libs doppler"
fi

# ----------------------------------------------------------------
# 2. Header accessible
# ----------------------------------------------------------------
echo ""
echo "--- headers ---"

if [ -f "${PREFIX}/include/doppler.h" ]; then
    ok "doppler.h found at ${PREFIX}/include/doppler.h"
else
    fail "doppler.h not found at ${PREFIX}/include/doppler.h"
fi

# ----------------------------------------------------------------
# 3. Library file exists
# ----------------------------------------------------------------
echo ""
echo "--- libraries ---"

if ls "${PREFIX}/lib/libdoppler"* 1>/dev/null 2>&1; then
    ok "libdoppler.{so,a} found in ${PREFIX}/lib/"
else
    fail "libdoppler not found in ${PREFIX}/lib/"
fi

# ----------------------------------------------------------------
# 4. Compile a minimal C program using pkg-config flags
# ----------------------------------------------------------------
echo ""
echo "--- compile smoke test ---"

SMOKETMP=$(mktemp -d)
trap 'rm -rf "$SMOKETMP"' EXIT

cat > "${SMOKETMP}/smoke.c" << 'EOF'
#include <doppler.h>
#include <stdio.h>
int main(void) {
    printf("doppler version: %d.%d.%d\n",
           DP_VERSION_MAJOR, DP_VERSION_MINOR, DP_VERSION_PATCH);
    return 0;
}
EOF

# Use --define-variable=prefix to override the prefix baked into the .pc file at
# cmake configure time.  Without this, pkg-config returns -I/usr/local/include
# even when the package is installed under a different prefix.
SMOKE_CFLAGS=$(pkg-config --define-variable=prefix="${PREFIX}" --cflags doppler 2>/dev/null \
               || echo "-I${PREFIX}/include")
SMOKE_LIBS=$(pkg-config --define-variable=prefix="${PREFIX}" --libs doppler 2>/dev/null \
             || echo "-L${PREFIX}/lib -ldoppler")
# Bake in rpath so the binary finds libdoppler.so without LD_LIBRARY_PATH
SMOKE_RPATH="-Wl,-rpath,${PREFIX}/lib"

# shellcheck disable=SC2086
if gcc -o "${SMOKETMP}/smoke" "${SMOKETMP}/smoke.c" \
       $SMOKE_CFLAGS $SMOKE_LIBS $SMOKE_RPATH 2>"${SMOKETMP}/gcc.err"; then
    ok "compiled with pkg-config flags"
else
    fail "compilation failed: $(cat "${SMOKETMP}/gcc.err")"
fi

if OUTPUT=$("${SMOKETMP}/smoke" 2>&1); then
    ok "smoke binary ran successfully: ${OUTPUT}"
else
    fail "smoke binary failed to run: ${OUTPUT}"
fi

# ----------------------------------------------------------------
# 5. CMake find_package
# ----------------------------------------------------------------
echo ""
echo "--- CMake find_package ---"

CMAKE_PREFIX="${PREFIX}/lib/cmake/doppler"
if [ -f "${CMAKE_PREFIX}/DopplerConfig.cmake" ] && \
   [ -f "${CMAKE_PREFIX}/dopplerTargets.cmake" ]; then
    ok "CMake config files found in ${CMAKE_PREFIX}/"
else
    fail "CMake config files not found in ${CMAKE_PREFIX}/ — find_package will fail"
fi

# ----------------------------------------------------------------
# Summary
# ----------------------------------------------------------------
echo ""
echo "========================================"
echo "Results: $((PASS+FAIL)) checks — ${PASS} passed, ${FAIL} failed"
echo "========================================"
echo ""

[ "$FAIL" -eq 0 ]
