#!/usr/bin/env bash
# LEGACY: pre-split-TOML bootstrap history. Objects now live in objects/*.toml.
# To regenerate: edit objects/<obj>.toml then: jm apply objects/<obj>.toml
# ddc_scaffold.sh — declare the ddc module interface via just-makeit.
#
# Run from: doppler-jm/ (project root, must contain just-makeit.toml)
#
# Prerequisites
# -------------
# native/src/ddc/ddc_core.c must exist (hand-written) BEFORE running this
# script so that --impl can lift ddc_execute / ddcr_execute bodies.  The
# file is committed to the repo; do not delete it first.
#
# Signal chains
# -------------
# DDC:  CF32 in  → LO mix → polyphase resample → CF32 out
# DDCR: float in → halfband R2C (embedded 19-tap Kaiser, fs/4 shift)
#                → LO mix → polyphase resample → CF32 out
#
# Both are streaming (variable block size per execute() call).
# ddc_core.c links: lo_core, resamp_core, hbdecim_r2c_core, m
#
# Generated (filled by this script + --impl):
#   native/inc/ddc/ddc_core.h        (overwritten — hand-restore from git)
#   native/src/ddc/ddc_core.c        (overwritten — hand-restore from git)
#   native/src/ddc/ddc_ext.c         (overwritten — hand-restore from git)
#   native/src/ddc/CMakeLists.txt    (overwritten — hand-restore from git)
#   src/doppler_jm/ddc/__init__.py
#   src/doppler_jm/ddc/ddc.pyi
#   src/doppler_jm/ddc/tests/test_ddc.py
#   src/doppler_jm/ddc/tests/test_ddcr.py
#   CMakeLists.txt                   (updated)
#   just-makeit.toml                 (updated)
#
# Hand-edit after scaffolding:
#   native/inc/ddc/ddc_core.h        ddc_state_t / ddcr_state_t are opaque;
#                                    restore full header with ddc_execute sig
#   native/src/ddc/ddc_core.c        restore ddcr_state struct, ddcr_create,
#                                    embedded s_hb_fir[19] array; --impl covers
#                                    ddc_execute / ddcr_execute bodies only
#   native/src/ddc/ddc_ext.c        DDC_init / DDCR_init: call ddc_create /
#                                    ddcr_create(norm_freq, rate); add rate
#                                    property getter backed by ddc_get_rate;
#                                    rename tp_name strings to "ddc.DDC" /
#                                    "ddc.DDCR"; add __enter__ / __exit__
#   native/src/ddc/CMakeLists.txt   add lo_core, resamp_core,
#                                    hbdecim_r2c_core, m to target_link_libraries

set -euo pipefail

JM="${JUST_MAKEIT:-just-makeit}"
DDC_CORE="native/src/ddc/ddc_core.c"

# ── Module ────────────────────────────────────────────────────────────────────
$JM module ddc

# ── DDC — complex-input digital down-converter ───────────────────────────────
# --no-state : ddc_state_t is hand-written (opaque struct; holds lo_state_t *
#              and resamp_state_t *).  jm cannot infer it.
# --no-step  : DDC has no single-sample step; execute is a block operation.
# DDC_init hand-edit: call ddc_create(norm_freq, rate) instead of generated stub.
$JM object ddc \
    --module ddc \
    --class-name DDC \
    --no-state \
    --no-step \
    --init-param "norm_freq:double" \
    --init-param "rate:double"

# execute: CF32 block in → CF32 block out (resampled, variable length).
# --impl lifts the ddc_execute body from ddc_core.c (lo_steps + multiply +
# resamp_execute).  The generated ext wrapper calls ddc_execute(self->state,
# in, n_in, out, max_out); the body fill is for the core stub only.
$JM method ddc execute \
    --module ddc \
    --param "x:float _Complex[]" \
    --return-type "float _Complex" \
    --variable-output \
    --impl "${DDC_CORE}::ddc_execute"

# reset: zero LO phase + resampler history.
$JM method ddc reset \
    --module ddc

# get_norm_freq / set_norm_freq: retune LO without resetting state.
$JM method ddc get_norm_freq \
    --module ddc \
    --return-type double

$JM method ddc set_norm_freq \
    --module ddc \
    --param "norm_freq:double"

# rate: read-only double — backed by ddc_get_rate(self->state).
# Hand-edit the generated getter to call ddc_get_rate instead of reading
# a struct field.
$JM property ddc rate \
    --module ddc \
    --type double

# ── DDCR — real-input digital down-converter ─────────────────────────────────
# Real float32 input; embedded 19-tap Kaiser halfband R2C (60 dB, 0.4/0.6)
# provides 2:1 decimation and fs/4 shift before LO mix + resample.
# rate must be in (0, 0.5) — ddcr_create returns NULL otherwise.
# DDCR_init hand-edit: call ddcr_create(norm_freq, rate).
$JM object ddcr \
    --module ddc \
    --class-name DDCR \
    --no-state \
    --no-step \
    --init-param "norm_freq:double" \
    --init-param "rate:double"

# execute: float32 real in → CF32 out.
# Input type "float[]" → NPY_FLOAT32.
# --impl lifts ddcr_execute body (hbdecim_r2c_execute → lo_steps → multiply
# → resamp_execute).
$JM method ddcr execute \
    --module ddc \
    --param "x:float[]" \
    --return-type "float _Complex" \
    --variable-output \
    --impl "${DDC_CORE}::ddcr_execute"

$JM method ddcr reset \
    --module ddc

$JM method ddcr get_norm_freq \
    --module ddc \
    --return-type double

$JM method ddcr set_norm_freq \
    --module ddc \
    --param "norm_freq:double"

# rate: total fs_out / fs_in (not the intermediate resampler rate).
# Hand-edit getter to call ddcr_get_rate(self->state).
$JM property ddcr rate \
    --module ddc \
    --type double

echo
echo "ddc scaffold done."
echo "Hand-restore native/src/ddc/ddc_core.c (opaque struct, embedded FIR, ddcr_create)."
echo "Hand-edit native/src/ddc/ddc_ext.c: DDC_init / DDCR_init, rate getter, tp_name strings."
echo "Hand-edit native/src/ddc/CMakeLists.txt: add lo_core resamp_core hbdecim_r2c_core m."
