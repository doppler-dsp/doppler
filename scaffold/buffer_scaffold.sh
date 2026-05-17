#!/usr/bin/env bash
# buffer_scaffold.sh — declare the buffer module interface via just-makeit.
#
# Run from: doppler-jm/ (project root, must contain just-makeit.toml)
#
# What buffer is
# --------------
# Three double-mapped SPSC circular buffer types for real-time IQ streaming.
# All three share the same lifecycle (create/destroy) and access pattern
# (write / wait / consume).  There is no buffer_core.c — every buffer
# operation is implemented in the platform-specific buffer.h (mmap-based).
# No --impl is possible.
#
# Types
# -----
#   F32Buffer(n)  — complex64  (float32 IQ pairs)
#   F64Buffer(n)  — complex128 (double64 IQ pairs)
#   I16Buffer(n)  — int16 IQ pairs; wait() returns shape (n, 2) int16
#
# All three take n_samples (power-of-2, page-aligned) as the sole init arg.
#
# Generated (skeleton only — hand-fill all method bodies):
#   native/src/buffer/buffer_ext.c   (overwritten — hand-restore from git)
#   native/src/buffer/CMakeLists.txt (overwritten — hand-restore from git)
#   src/doppler_jm/buffer/__init__.py
#   src/doppler_jm/buffer/buffer.pyi
#   src/doppler_jm/buffer/tests/test_buffer.py
#   CMakeLists.txt                   (updated)
#   just-makeit.toml                 (updated)
#
# No native/inc/buffer/buffer_core.h or buffer_core.c are generated — the
# public header is native/inc/buffer/buffer.h (hand-written, wraps mmap).
#
# Hand-edit after scaffolding
# ---------------------------
# buffer_ext.c is almost entirely hand-crafted.  The generated skeleton
# provides struct layout and PyTypeObject shells; fill:
#
#   F32Buffer_init   dp_f32_create(n_samples)
#   F32Buffer_write  dp_f32_write()  — takes complex64 ndarray, returns bool
#   F32Buffer_wait   dp_f32_wait()   — releases GIL; returns zero-copy view
#   F32Buffer_consume dp_f32_consume() — optional n arg, defaults to wait_n
#   F32Buffer_destroy dp_f32_destroy()
#   F32Buffer_capacity / _dropped — read buf->capacity / buf->dropped
#   (same pattern for F64Buffer / I16Buffer with appropriate dtype guards)
#
#   I16Buffer_wait: shape (n, 2) int16, not 1-D complex
#   I16Buffer_write: validates total size is even (IQ pairs)
#
# CMakeLists.txt: no extra OBJECT libs needed; link only Python3::NumPy.
#   buffer.h uses Linux mmap; add -D_GNU_SOURCE if not already set.

set -euo pipefail

JM="${JUST_MAKEIT:-just-makeit}"

# ── Module ────────────────────────────────────────────────────────────────────
$JM module buffer

# ── F32Buffer — complex64 (float32 IQ) ───────────────────────────────────────
# --no-state : F32BufferObject holds dp_f32 * + npy_intp wait_n; hand-fill.
# --no-step  : no single-sample step; access is write/wait/consume.
# F32Buffer_init hand-edit: parse Py_ssize_t n_samples, call dp_f32_create.
$JM object f32buffer \
    --module buffer \
    --class-name F32Buffer \
    --no-state \
    --no-step \
    --init-param "n_samples:size_t"

# write: non-blocking write from a complex64 ndarray.  Returns bool.
# jm models this as "float _Complex[]" input, void return; hand-edit to
# return PyBool_FromLong and call dp_f32_write(buf, data, n).
$JM method f32buffer write \
    --module buffer \
    --param "x:float _Complex[]"

# wait: block until n samples available; return zero-copy complex64 view.
# jm models as variable-output CF32; hand-edit to:
#   release GIL (Py_BEGIN_ALLOW_THREADS), call dp_f32_wait, regain GIL,
#   wrap pointer with PyArray_SimpleNewFromData + SetBaseObject.
$JM method f32buffer wait \
    --module buffer \
    --param "n:size_t" \
    --return-type "float _Complex" \
    --variable-output

# consume: release n samples (defaults to last wait_n when n omitted).
# jm generates a fixed-arg wrapper; hand-edit to use "|n" PyArg_ParseTuple.
$JM method f32buffer consume \
    --module buffer \
    --param "n:size_t"

# destroy: explicit unmap.  dealloc also calls this; idempotent (NULL guard).
$JM method f32buffer destroy \
    --module buffer

# capacity / dropped: read-only struct fields on dp_f32 *.
$JM property f32buffer capacity \
    --module buffer \
    --type size_t

$JM property f32buffer dropped \
    --module buffer \
    --type size_t

# ── F64Buffer — complex128 (double64 IQ) ─────────────────────────────────────
$JM object f64buffer \
    --module buffer \
    --class-name F64Buffer \
    --no-state \
    --no-step \
    --init-param "n_samples:size_t"

$JM method f64buffer write \
    --module buffer \
    --param "x:double _Complex[]"

$JM method f64buffer wait \
    --module buffer \
    --param "n:size_t" \
    --return-type "double _Complex" \
    --variable-output

$JM method f64buffer consume \
    --module buffer \
    --param "n:size_t"

$JM method f64buffer destroy \
    --module buffer

$JM property f64buffer capacity \
    --module buffer \
    --type size_t

$JM property f64buffer dropped \
    --module buffer \
    --type size_t

# ── I16Buffer — int16 IQ pairs ───────────────────────────────────────────────
# wait() returns shape (n, 2) int16 (col 0 = I, col 1 = Q).
# write() validates total element count is even before calling dp_i16_write.
# jm has no native int16 array type; hand-edit write/wait bodies entirely.
$JM object i16buffer \
    --module buffer \
    --class-name I16Buffer \
    --no-state \
    --no-step \
    --init-param "n_samples:size_t"

# write: hand-edit — accept any int16 C-contiguous ndarray (1-D or (n,2)),
# validate even element count, call dp_i16_write.
$JM method i16buffer write \
    --module buffer \
    --param "x:float _Complex[]"  # placeholder type; hand-edit to int16 ndarray

# wait: hand-edit — shape (n, 2) int16 output, not 1-D complex.
$JM method i16buffer wait \
    --module buffer \
    --param "n:size_t" \
    --return-type "float _Complex" \
    --variable-output  # placeholder; hand-edit return to NPY_INT16 (n,2)

$JM method i16buffer consume \
    --module buffer \
    --param "n:size_t"

$JM method i16buffer destroy \
    --module buffer

$JM property i16buffer capacity \
    --module buffer \
    --type size_t

$JM property i16buffer dropped \
    --module buffer \
    --type size_t

echo
echo "buffer scaffold done."
echo "Restore native/src/buffer/buffer_ext.c from git — generated skeleton needs"
echo "full hand-written bodies for write/wait/consume/destroy and all three types."
echo "No --impl available: all logic is in native/inc/buffer/buffer.h (mmap routines)."
