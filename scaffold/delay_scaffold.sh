#!/usr/bin/env bash
# delay_scaffold.sh — declare the delay module interface via just-makeit.
#
# Run from: doppler-jm/   (project root, must contain just-makeit.toml)
#
# Reference algorithm (read-only — do not wrap):
#   ../native/src/delay/delay_core.c   dual-buffer circular delay line
#
# Generated (~14 files — do not edit):
#   native/inc/delay/delay_core.h
#   native/src/delay/delay_core.c
#   native/src/delay/CMakeLists.txt
#   native/inc/delay/delay_core.h      (module umbrella)
#   native/benchmarks/bench_delay_core.c
#   native/tests/test_delay_core.c
#   src/doppler_jm/delay/__init__.py
#   src/doppler_jm/delay/delay.pyi
#   src/doppler_jm/delay/tests/test_delay.py
#   src/doppler_jm/delay/benchmarks/bench_delay.py
#   CMakeLists.txt                     (updated by jm)
#   just-makeit.toml                   (updated by jm)
#
# Hand-edit (4 files):
#   native/inc/delay/delay_core.h
#     — struct fields: double _Complex *buf; size_t head, capacity, num_taps, mask
#     — delay_create(size_t num_taps)  — capacity = next power-of-two >= num_taps
#
#   native/src/delay/delay_core.c
#     — delay_create: calloc(2*capacity), set fields
#     — delay_destroy: free buf, free state
#     — delay_reset: memset buf 0, head = 0
#     — delay_push: head = (head-1) & mask; write both halves
#     — delay_ptr_max_out: return state->num_taps
#     — delay_ptr: memcpy num_taps elements from &buf[head] into out
#     — delay_push_ptr_max_out: return state->num_taps
#     — delay_push_ptr: push then memcpy window into out
#     — delay_write: loop calling delay_push
#
#   native/tests/test_delay_core.c
#     — lifecycle, push/ptr round-trip, continuity across blocks,
#       write batch, capacity is power-of-two, reset clears buffer
#
#   src/doppler_jm/delay/tests/test_delay.py
#     — mirror C tests at the Python layer
#
set -euo pipefail

JM="${JUST_MAKEIT:-just-makeit}"

# ── Module ────────────────────────────────────────────────────────────────────
$JM module delay

# ── Object ────────────────────────────────────────────────────────────────────
# Dual-buffer circular delay line for complex128 IQ samples.
#
# --no-state : 5 fields (buf*, head, capacity, num_taps, mask); jm cannot infer
# --no-step  : no single-sample step returning a scalar; all output via ptr()
# --init-param num_taps: window length; create rounds up to next power-of-two
$JM object delay \
    --module delay \
    --class-name DelayCf64 \
    --no-state \
    --no-step \
    --init-param "num_taps:size_t:1"

# ── Methods ───────────────────────────────────────────────────────────────────

# push: insert one complex128 sample; head decrements, both halves written.
# --param "x:double _Complex" : scalar input (not an array)
# void return: sink operation, no output.
$JM method delay push \
    --module delay \
    --param "x:double _Complex" \
    --return-type void

# ptr: return a copy of the current num_taps-element tap window.
# Newest sample is at index 0; oldest at index num_taps-1.
# --arg-type void       : no input
# --variable-output     : output length = ptr_max_out() = num_taps
$JM method delay ptr \
    --module delay \
    --arg-type void \
    --return-type "double _Complex" \
    --variable-output

# push_ptr: push one sample and return the updated window in one call.
# Scalar --param input combined with --variable-output array return.
$JM method delay push_ptr \
    --module delay \
    --param "x:double _Complex" \
    --return-type "double _Complex" \
    --variable-output

# write: push an array of samples (batch push); no output.
# --arg-type "double _Complex" : primary array input x[0..n-1]
# void return: sink.
$JM method delay write \
    --module delay \
    --arg-type "double _Complex" \
    --return-type void

# ── Properties ────────────────────────────────────────────────────────────────

# num_taps: window length passed at construction.
# --field: jm reads state->num_taps directly (no generated getter function).
$JM property delay num_taps \
    --module delay \
    --type size_t \
    --field

# capacity: smallest power-of-two >= num_taps (actual buffer half-length).
$JM property delay capacity \
    --module delay \
    --type size_t \
    --field

echo
echo "delay scaffold done. Hand-edit the 4 files listed above."
