#!/usr/bin/env bash
# agc_scaffold.sh — declare the agc module interface via just-makeit.
#
# Run from: doppler/   (project root, must contain just-makeit.toml)
#
# Component: log-domain automatic gain control (AGC).
#   - linear-in-dB gain   : g_lin = 10^(gain_db/20)
#   - power detector      : exponential moving average of |y|^2
#   - 1st-order loop filter: gain_db += loop_gain * (ref_db - measured_db)
# Feedback topology — power is detected AFTER the gain.
#
# Generated (~12 files — do not edit):
#   native/inc/agc/agc_core.h
#   native/src/agc/agc_core.c
#   native/src/agc/CMakeLists.txt
#   native/benchmarks/bench_agc_core.c
#   native/tests/test_agc_core.c
#   src/doppler_jm/agc/__init__.py
#   src/doppler_jm/agc/agc.pyi
#   src/doppler_jm/agc/tests/test_agc.py
#   src/doppler_jm/agc/benchmarks/bench_agc.py
#   CMakeLists.txt                    (updated by jm)
#   just-makeit.toml                  (updated by jm)
#
# Hand-edit (4 files):
#   native/inc/agc/agc_core.h
#     — docstring: algorithm + lifecycle
#     — agc_step() body: apply gain, detect power, run loop filter
#   native/src/agc/agc_core.c
#     — agc_create / agc_reset: p_avg initialised to the reference power
#       10^(ref_db/10) so the loop starts settled (no -inf log10 transient)
#   native/tests/test_agc_core.c
#     — convergence to ref_db, level-independent settling time, reset
#   src/doppler_jm/agc/tests/test_agc.py
#     — mirror the C tests at the Python layer
#
set -euo pipefail

JM="${JUST_MAKEIT:-just-makeit}"

# -- Module --------------------------------------------------------------------
$JM module agc

# -- Object --------------------------------------------------------------------
# cf32 -> cf32 per-sample gain control.
# --mutable     : step() mutates gain_db and p_avg every sample.
# --no-state    : struct is hand-filled — constructor config (ref_db,
#                 loop_gain, alpha) plus internal loop state (gain_db,
#                 p_avg) that must NOT be constructor params.
# --init-param  : constructor configuration only.
$JM object agc \
    --module agc \
    --class-name AGC \
    --arg-type "float _Complex" \
    --return-type "float _Complex" \
    --mutable \
    --no-state \
    --init-param "ref_db:double:0.0" \
    --init-param "loop_gain:double:0.01" \
    --init-param "alpha:double:0.05"

# -- Properties ----------------------------------------------------------------
# gain_db: current loop-filter integrator value (read-only — observe the loop).
$JM property agc gain_db \
    --module agc \
    --type double \
    --field

# ref_db / loop_gain / alpha: retunable at runtime.
$JM property agc ref_db \
    --module agc \
    --type double \
    --field \
    --writable

$JM property agc loop_gain \
    --module agc \
    --type double \
    --field \
    --writable

$JM property agc alpha \
    --module agc \
    --type double \
    --field \
    --writable

echo
echo "agc scaffold done. Hand-edit the 4 files listed above."
