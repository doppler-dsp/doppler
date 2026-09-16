#!/usr/bin/env bash
# Regenerate the jm-owned buffer binding into ./generated/.
#
# Nothing here touches doppler's build. It scaffolds a throwaway project,
# applies the declaration beside this script, and copies the generated glue
# back so it can be diffed against the hand-written binding in
# native/src/buffer/.
#
#   ./regenerate.sh && diff -u ../../native/src/buffer/buffer_ext.c \
#                             generated/buffer_ext_f32_buffer.c
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# NOTE: needs a just-makeit NEWER than doppler's pinned 0.75.5 -- see README.
just-makeit new dpring --quiet >/dev/null 2>&1 || just-makeit new dpring >/dev/null
cd "$tmp"/dpring 2>/dev/null || { cd "$tmp" && just-makeit new dpring >/dev/null && cd dpring; }
just-makeit module buffer >/dev/null
cp "$here/f32_buffer.toml" objects/
just-makeit apply >/dev/null

mkdir -p "$here/generated"
cp native/src/buffer/buffer_ext_f32_buffer.c "$here/generated/"
cp src/dpring/buffer/buffer.pyi "$here/generated/"

# NO raw clang-format here: the Makefile is the SSOT for how every tool runs,
# and this repo already formats jm-generated C two ways that agree -- jm's own
# `c_format_command` on apply, and the pre-commit hook on commit. Committing
# the result is what normalises it; a third invocation here would be a second
# opinion about style that can drift from both.
#
#   ./regenerate.sh && git add -A && git commit    # pre-commit formats it
#
echo "regenerated into $here/generated/"
