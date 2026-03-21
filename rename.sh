#!/usr/bin/env bash
# rename.sh — doppler → doppler, dp_ → dp_
# Run from the root of the doppler repo
# Safe: dry-run by default, pass --apply to execute

set -euo pipefail

APPLY=false
if [[ "${1:-}" == "--apply" ]]; then
  APPLY=true
fi

log() { echo "  $1"; }
drylog() { echo "  [dry-run] $1"; }

run() {
  if $APPLY; then
    log "$1"
    eval "$2"
  else
    drylog "$1"
  fi
}

echo ""
echo "=== Doppler Rename Script ==="
echo "Mode: $($APPLY && echo 'APPLY' || echo 'DRY RUN')"
echo ""

# ─────────────────────────────────────────────
# 1. DIRECTORY RENAMES
# ─────────────────────────────────────────────
echo "── Directory renames"

run "python/doppler → python/doppler" \
  "mv python/doppler python/doppler"

run "c/include/dp → c/include/dp" \
  "mv c/include/dp c/include/dp"

echo ""

# ─────────────────────────────────────────────
# 2. FILE RENAMES
# ─────────────────────────────────────────────
echo "── File renames"

run "c/cmake/doppler.pc.in → doppler.pc.in" \
  "mv c/cmake/doppler.pc.in c/cmake/doppler.pc.in"

run "c/cmake/DopplerConfig.cmake.in → DopplerConfig.cmake.in" \
  "mv c/cmake/DopplerConfig.cmake.in c/cmake/DopplerConfig.cmake.in"

run "python/src/dp_buffer.c → dp_buffer.c" \
  "mv python/src/dp_buffer.c python/src/dp_buffer.c"

run "python/src/dp_fft.c → dp_fft.c" \
  "mv python/src/dp_fft.c python/src/dp_fft.c"

run "python/src/dp_stream.c → dp_stream.c" \
  "mv python/src/dp_stream.c python/src/dp_stream.c"

run "c/include/doppler.h → c/include/doppler.h" \
  "mv c/include/doppler.h c/include/doppler.h"

echo ""

# ─────────────────────────────────────────────
# 3. CONTENT SUBSTITUTIONS
# ─────────────────────────────────────────────
echo "── Content substitutions"

# File extensions to process
EXTS="c|h|cc|cmake|toml|md|yml|yaml|txt|in|pc|rs|py|sh"

# Build list of files to process — exclude build artifacts
FILES=$(find . \
  -type f \
  -regextype posix-extended \
  -regex ".*\.($EXTS)" \
  ! -path "./CMakeFiles/*" \
  ! -path "./dist/*" \
  ! -path "./site/*" \
  ! -path "./.git/*" \
  ! -path "./ffi/rust/target/*" \
  ! -path "./python/vendor/*" \
  2>/dev/null)

# Substitution pairs — order matters, more specific first
declare -a SUBS=(
  # C symbol prefixes
  "dp_buffer|dp_buffer"
  "dp_fft|dp_fft"
  "dp_stream|dp_stream"
  "DP_BUFFER|DP_BUFFER"
  "DP_FFT|DP_FFT"
  "DP_STREAM|DP_STREAM"
  "DP_|DP_"

  # Include guards and headers
  "dp\.h|doppler.h"
  "include/dp/|include/dp/"
  "\"dp/|\"dp/"
  "<dp/|<dp/"

  # CMake and pkg-config
  "DopplerConfig|DopplerConfig"
  "doppler\.pc|doppler.pc"
  "Doppler|Doppler"
  "DOPPLER|DOPPLER"
  "doppler|doppler"
)

apply_subs() {
  local file="$1"
  local tmpfile=$(mktemp)
  cp "$file" "$tmpfile"
  for pair in "${SUBS[@]}"; do
    local from="${pair%%|*}"
    local to="${pair##*|}"
    sed -i "s|${from}|${to}|g" "$tmpfile"
  done
  if ! diff -q "$tmpfile" "$file" > /dev/null 2>&1; then
    if $APPLY; then
      cp "$tmpfile" "$file"
      log "updated: $file"
    else
      drylog "would update: $file"
      diff "$file" "$tmpfile" | grep '^[<>]' | head -5
    fi
  fi
  rm "$tmpfile"
}

for f in $FILES; do
  apply_subs "$f"
done

echo ""

# ─────────────────────────────────────────────
# 4. RUST CRATE RENAME
# ─────────────────────────────────────────────
echo "── Rust crate"

if $APPLY; then
  if grep -q "doppler" ffi/rust/Cargo.toml 2>/dev/null; then
    sed -i 's/doppler/doppler-dsp/g' ffi/rust/Cargo.toml
    log "updated ffi/rust/Cargo.toml"
  fi
else
  drylog "would update ffi/rust/Cargo.toml crate name → doppler-dsp"
fi

echo ""

# ─────────────────────────────────────────────
# 5. PYTHON PACKAGE STRUCTURE
# ─────────────────────────────────────────────
echo "── Python package layout"
run "python/ → src/ (uv convention)" \
  "mv python doppler_python_tmp && mv doppler_python_tmp src"

echo ""

# ─────────────────────────────────────────────
# 6. CLEANUP
# ─────────────────────────────────────────────
echo "── Cleanup build artifacts"
run "remove dist/" "rm -rf dist/"
run "remove site/" "rm -rf site/"
run "remove CMakeFiles/" "rm -rf CMakeFiles/"
run "remove uv.lock (regenerate after rename)" "rm -f uv.lock"

echo ""
echo "=== Done ==="
if ! $APPLY; then
  echo ""
  echo "  This was a dry run. Review the output above then:"
  echo "  bash rename.sh --apply"
  echo ""
  echo "  After applying:"
  echo "  1. Update pyproject.toml manually (see notes)"
  echo "  2. uv lock"
  echo "  3. cmake -B build && cmake --build build  (verify C builds)"
  echo "  4. uv run pytest  (verify Python tests pass)"
  echo "  5. git add -A && git commit -m 'rename: doppler → doppler'"
fi
