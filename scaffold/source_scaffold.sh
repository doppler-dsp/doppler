#!/usr/bin/env bash
# LEGACY: pre-split-TOML bootstrap history. Objects now live in objects/*.toml.
# To regenerate: edit objects/<obj>.toml then: jm apply objects/<obj>.toml
# source_scaffold.sh — declare the source module interface via just-makeit.
#
# Run from: doppler-jm/   (project root, must contain just-makeit.toml)
#
# Reference algorithms (read-only — do not wrap):
#   ../native/src/nco/nco_core.c   old doppler NCO
#   ../native/src/lo/lo_core.c     old doppler LO + 2^16 LUT
#
# Generated (~18 files — do not edit):
#   native/inc/nco/nco_core.h
#   native/src/nco/nco_core.c        NCO execute bodies lifted via --impl
#   native/inc/lo/lo_core.h
#   native/src/lo/lo_core.c
#   native/src/source/source_ext.c
#   native/src/source/source_core.c
#   native/src/source/CMakeLists.txt
#   native/inc/source/source_core.h
#   native/benchmarks/bench_nco_core.c
#   native/benchmarks/bench_lo_core.c
#   native/tests/test_nco_core.c
#   native/tests/test_lo_core.c
#   src/doppler_jm/source/__init__.py
#   src/doppler_jm/source/source.pyi
#   src/doppler_jm/source/tests/test_nco.py
#   src/doppler_jm/source/tests/test_lo.py
#   src/doppler_jm/source/benchmarks/bench_nco.py
#   src/doppler_jm/source/benchmarks/bench_lo.py
#   CMakeLists.txt                    (updated by jm)
#   just-makeit.toml                  (updated by jm)
#
# Hand-edit (6 files):
#   native/inc/nco/nco_core.h
#     — struct fields: uint32_t phase, phase_inc, nmax; double norm_freq
#     — NCO_ADD_OVF macro (wrapping add with carry flag)
#     — declare: nco_create, nco_destroy, nco_reset
#                nco_steps_u32, nco_steps_u32_max_out
#                nco_steps_u32_scaled, nco_steps_u32_scaled_max_out
#                nco_steps_u32_ovf, nco_steps_u32_ovf_max_out
#                nco_get/set_norm_freq, nco_get/set_phase, nco_get_phase_inc
#
#   native/src/nco/nco_core.c
#     — nco_create: phase_inc = (uint32_t)(norm_freq * 4294967296.0)
#     — nco_reset: zero phase only
#     — nco_steps_u32_max_out: return SIZE_MAX (unlimited generator)
#     — nco_steps_u32_scaled_max_out, nco_steps_u32_ovf_max_out: same
#     — execute bodies are lifted from reference via --impl; just fill create/reset/max_out
#
#   native/inc/lo/lo_core.h
#     — struct fields: uint32_t phase, phase_inc; double norm_freq
#     — declare: lo_create, lo_destroy, lo_reset
#                lo_steps, lo_steps_max_out
#                lo_steps_ctrl, lo_steps_ctrl_max_out
#                lo_get/set_norm_freq, lo_get/set_phase, lo_get_phase_inc
#
#   native/src/lo/lo_core.c
#     — static 2^16 float sin/cos LUT, lazy-init (shared across all instances)
#     — lo_steps: LUT[phase >> 16] → (cos, sin) as CF32 phasor; output BEFORE increment
#     — lo_steps_ctrl: ctrl[i] (real float) added to norm_freq per sample; base unchanged
#
#   native/tests/test_nco_core.c
#     — lifecycle, zero-freq (constant phase), quarter-rate sequence,
#       phase continuity across blocks, nmax scaling, ovf carry flag
#
#   native/tests/test_lo_core.c
#     — DC tone (all 1+0j), quarter-rate IQ values, phase continuity,
#       ctrl-port frequency shift, SFDR > 90 dBc via integer FFT bin
#
# Note: source.pyi still needs n: int = 1 added to generator stubs (sed patch below).
#
set -euo pipefail

JM="${JUST_MAKEIT:-just-makeit}"

# ── Module ────────────────────────────────────────────────────────────────────
$JM module source

# ── NCO ───────────────────────────────────────────────────────────────────────
# Pure 32-bit phase accumulator — no LUT, no trig.  Generator pattern:
# no input array on primary methods, output length driven by 'n' arg.
# --class-name NCO: sets the Python class name, tp_name, test imports, and
#                   pyi class name throughout — no sed patches needed.
# --no-state : 4+ fields (phase, phase_inc, norm_freq, nmax) — fill by hand
# --no-step  : all output via named block methods below
$JM object nco \
    --module source \
    --class-name NCO \
    --no-state \
    --no-step \
    --init-param "norm_freq:double:0.0" \
    --init-param "nmax:uint32_t:0"

# Advance n samples; return raw uint32 phase accumulator values.
# --arg-type void: no bulk input; jm generates (Py_ssize_t n) in place of array.
# --impl lifts nco_execute_u32 body; --replace adapts param name nco→state.
# jm appends return n; automatically when reference is void but stub is size_t.
$JM method nco steps_u32 \
    --module source \
    --arg-type void \
    --return-type uint32_t \
    --variable-output \
    --impl "../native/src/nco/nco_core.c::nco_execute_u32" \
    --replace "nco::state"

# Same but each value scaled into [0, nmax): (uint64_t)phase * nmax >> 32.
$JM method nco steps_u32_scaled \
    --module source \
    --arg-type void \
    --return-type uint32_t \
    --variable-output \
    --impl "../native/src/nco/nco_core.c::nco_execute_u32_scaled" \
    --replace "nco::state"

# Return (uint32[], uint8[]): raw phase + per-sample wrap-around carry flag.
# NCO_ADD_OVF macro must be declared in nco_core.h.
# jm uses out1 for the second buffer — --replace carry::out1.
$JM method nco steps_u32_ovf \
    --module source \
    --arg-type void \
    --return-type uint32_t \
    --variable-output \
    --multi-output uint8_t \
    --impl "../native/src/nco/nco_core.c::nco_execute_u32_ovf" \
    --replace "nco::state" \
    --replace "carry::out1"

$JM property nco norm_freq \
    --module source \
    --type double \
    --writable

$JM property nco phase \
    --module source \
    --type uint32_t \
    --writable

# Read-only: phase_inc = (uint32_t)(norm_freq * 2^32); set by create/set_norm_freq.
$JM property nco phase_inc \
    --module source \
    --type uint32_t

# ── LO ────────────────────────────────────────────────────────────────────────
# NCO + static 2^16 sin/cos LUT → CF32 phasors.  Same generator pattern.
# --class-name LO: same benefits as --class-name NCO above.
# --no-state : 3 fields (phase, phase_inc, norm_freq) + shared static LUT
# --no-step  : block-only output via steps / steps_ctrl
$JM object lo \
    --module source \
    --class-name LO \
    --no-state \
    --no-step \
    --init-param "norm_freq:double:0.0"

# Generate n CF32 phasors at current norm_freq (no input array).
# No --impl: lo_execute_cf32 references a static 2^16 LUT and LUT_QTR/LUT_SZ
# constants declared at file scope; --impl lifts bodies only, not those statics.
$JM method lo steps \
    --module source \
    --arg-type void \
    --return-type "float _Complex" \
    --variable-output

# Generate len(ctrl) CF32 phasors with per-sample FM deviation.
# ctrl[i] (real float) is added to norm_freq for sample i; base unchanged.
# No --impl for the same reason (static LUT dependency).
$JM method lo steps_ctrl \
    --module source \
    --param "ctrl:float[]" \
    --return-type "float _Complex" \
    --variable-output

$JM property lo norm_freq \
    --module source \
    --type double \
    --writable

$JM property lo phase \
    --module source \
    --type uint32_t \
    --writable

$JM property lo phase_inc \
    --module source \
    --type uint32_t

# ── Fix source.pyi: add n param to generator method stubs ────────────────────
# jm stubs variable-output --arg-type void methods as foo(self) -> ndarray.
# The correct signature is foo(self, n: int = 1) -> ndarray.
PYI="src/doppler_jm/source/source.pyi"
sed -i \
    -e 's/def steps_u32(self) ->/def steps_u32(self, n: int = 1) ->/' \
    -e 's/def steps_u32_scaled(self) ->/def steps_u32_scaled(self, n: int = 1) ->/' \
    -e 's/def steps_u32_ovf(self) ->/def steps_u32_ovf(self, n: int = 1) ->/' \
    -e 's/def steps(self) ->/def steps(self, n: int = 1) ->/' \
    "$PYI"

echo
echo "source scaffold done. Hand-edit the 6 files listed above."
