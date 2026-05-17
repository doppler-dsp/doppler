#!/usr/bin/env bash
# filter_scaffold.sh — declare the filter module interface via just-makeit.
#
# Run from: doppler-jm/ (project root, must contain just-makeit.toml)
#
# Reference algorithm: ../native/src/fir/fir_core.c (read-only reference)
#
# Generated (14 files — do not edit):
#   native/inc/fir/fir_core.h        state struct skeleton + fn decls
#   native/src/fir/fir_core.c        create/destroy/reset/execute stubs
#   native/src/filter/filter_ext.c   Python C extension glue
#   native/src/filter/CMakeLists.txt
#   native/src/filter/filter_core.c
#   native/inc/filter/filter_core.h
#   native/benchmarks/bench_fir_core.c
#   native/tests/test_fir_core.c
#   src/doppler_jm/filter/__init__.py
#   src/doppler_jm/filter/filter.pyi
#   src/doppler_jm/filter/tests/test_fir.py
#   src/doppler_jm/filter/benchmarks/bench_fir.py
#   CMakeLists.txt                    (updated by jm)
#   just-makeit.toml                  (updated by jm)
#
# Hand-edit (6 files):
#   native/inc/fir/fir_core.h        add struct fields; declare fir_create_real
#   native/src/fir/fir_core.c        fir_create, fir_create_real, ensure_scratch,
#                                    AVX-512 + scalar inner loops, execute_core variants
#   native/src/filter/filter_ext.c   Fir_init: dtype dispatch (float32→create_real,
#                                    complex64→create); rest is generated correctly
#   native/tests/test_fir_core.c     C-level smoke tests for the inner loops
#   src/doppler_jm/filter/tests/test_fir.py   real FIR test cases
#   src/doppler_jm/filter/filter.pyi          fix execute() return annotation

set -euo pipefail

JM="${JUST_MAKEIT:-just-makeit}"

# ── Module ───────────────────────────────────────────────────────────────────
$JM module filter

# ── Object ───────────────────────────────────────────────────────────────────
# --no-state : fir_state_t has 6 fields (taps/rtaps/delay/scratch/cap/num_taps)
#              that jm cannot infer; we fill the struct by hand
# --no-step  : FIR has no single-sample step; execute is a block operation
# Fir_init accepts a taps ndarray (float32 or complex64); hand-edit the
# generated init stub to dispatch float32→fir_create_real vs complex64→fir_create.
$JM object fir \
    --module filter \
    --no-state \
    --no-step

# ── Methods ──────────────────────────────────────────────────────────────────
# execute: CF32 block in → CF32 block out, same length (direct-form FIR is 1:1).
# Variable-output: ext.c allocates a grow-on-demand buffer; returns zero-copy view.
$JM method fir execute \
    --module filter \
    --arg-type "float _Complex" \
    --return-type "float _Complex" \
    --variable-output

# reset is already generated as a lifecycle method by 'jm object' — do not
# add it again with 'jm method'.  Hand-fill the <<IMPLEMENT>> body:
#   if (state->delay && state->num_taps > 1)
#       memset(state->delay, 0, (state->num_taps - 1) * sizeof(float _Complex));

# ── Properties ───────────────────────────────────────────────────────────────
# num_taps: direct struct field — --field avoids generating a separate getter fn.
$JM property fir num_taps \
    --module filter \
    --type size_t \
    --field

# is_real: rtaps != NULL — needs a getter; implement fir_is_real() in fir_core.c.
$JM property fir is_real \
    --module filter \
    --type bool

echo
echo "filter scaffold done. Hand-edit the 6 files listed above."
