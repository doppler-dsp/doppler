#!/usr/bin/env bash
# LEGACY: pre-split-TOML bootstrap history. Objects now live in objects/*.toml.
# To regenerate: edit objects/<obj>.toml then: jm apply objects/<obj>.toml
# filter_scaffold.sh — declare the filter module interface via just-makeit.
#
# Run from: doppler-jm/ (project root, must contain just-makeit.toml)
#
# Reference algorithms (read-only):
#   ../native/src/fir/fir_core.c
#   ../native/src/cic/cic_core.c
#
# Generated (do not edit):
#   native/inc/fir/fir_core.h        state struct skeleton + fn decls
#   native/src/fir/fir_core.c        create/destroy/reset/execute stubs
#   native/inc/cic/cic_core.h        state struct skeleton + fn decls
#   native/src/cic/cic_core.c        create/destroy/reset/decimate stubs
#   native/src/filter/filter_ext.c   Python C extension glue (FIR + CIC)
#   native/src/filter/CMakeLists.txt
#   native/src/filter/filter_core.c
#   native/inc/filter/filter_core.h
#   native/benchmarks/bench_fir_core.c
#   native/benchmarks/bench_cic_core.c
#   native/tests/test_fir_core.c
#   native/tests/test_cic_core.c
#   src/doppler_jm/filter/__init__.py
#   src/doppler_jm/filter/filter.pyi
#   src/doppler_jm/filter/tests/test_fir.py
#   src/doppler_jm/filter/tests/test_cic.py
#   src/doppler_jm/filter/benchmarks/bench_fir.py
#   src/doppler_jm/filter/benchmarks/bench_cic.py
#   CMakeLists.txt                    (updated by jm)
#   just-makeit.toml                  (updated by jm)
#
# Hand-edit:
#   native/inc/fir/fir_core.h        add struct fields; declare fir_create_real
#   native/src/fir/fir_core.c        fir_create, fir_create_real, ensure_scratch,
#                                    AVX-512 + scalar inner loops, execute_core variants
#   native/inc/cic/cic_core.h        add cic_state_t fields (integ/comb/phase/scales)
#   native/src/cic/cic_core.c        max_scale, cic_create, cic_decimate hot loop,
#                                    cic_reconfigure with comb realloc
#   native/src/filter/filter_ext.c   FIRObj_init: dtype dispatch (float32→create_real,
#                                    complex64→create); rest is generated correctly
#   native/tests/test_fir_core.c     C-level smoke tests for the inner loops
#   native/tests/test_cic_core.c     C-level correctness tests (DC, impulse response)
#   src/doppler_jm/filter/tests/test_fir.py   real FIR test cases
#   src/doppler_jm/filter/tests/test_cic.py   DC/tone/parametric CIC test cases
#   src/doppler_jm/filter/filter.pyi          fix execute/decimate return annotations

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

# ── Object: CIC ──────────────────────────────────────────────────────────────
# --no-state : cic_state_t fields (integ/comb arrays, R/N/M/phase/scales)
#              cannot be inferred; struct is hand-written in cic_core.h
# --no-step  : CIC has no single-sample step; decimate is a block operation
# --class-name CIC : Python class name (default would be "Cic")
# init params are the three constructor knobs; all have sensible defaults.
$JM object cic \
    --module filter \
    --class-name CIC \
    --no-state \
    --no-step \
    --init-param "R:uint32_t:1" \
    --init-param "N:uint32_t:4" \
    --init-param "M:uint32_t:1"

# decimate: CF32 block in → CF32 block out (decimated, variable length).
# --variable-output: ext.c allocates a lazy output buffer on first call
# (sized to n_in, always ≥ actual output count n_in/R); reused thereafter.
# Block size must stay consistent after the first call.
$JM method cic decimate \
    --module filter \
    --arg-type "float _Complex" \
    --return-type "float _Complex" \
    --variable-output

# reconfigure: change R/N/M in place; resets all filter state.
# Silently ignores invalid args (R=0, N>6, M>2, OOM) — not an error path.
$JM method cic reconfigure \
    --module filter \
    --arg-type void \
    --return-type void \
    --param "R:uint32_t" \
    --param "N:uint32_t" \
    --param "M:uint32_t"

# reset is generated as a lifecycle method by 'jm object' — do not re-add.

# Properties: direct struct fields — --field avoids generating a getter fn.
$JM property cic R           --module filter --type uint32_t --field
$JM property cic N           --module filter --type uint32_t --field
$JM property cic M           --module filter --type uint32_t --field
$JM property cic input_scale --module filter --type double   --field
$JM property cic output_scale --module filter --type double  --field

echo
echo "filter scaffold done. Hand-edit the 6 files listed above."
