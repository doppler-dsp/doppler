#!/usr/bin/env bash
# corr_scaffold.sh — declare the Corr and Corr2D objects via just-makeit.
#
# Run from: doppler/   (project root, must contain just-makeit.toml)
#
# Reference algorithm (read-only — do not wrap):
#   native/src/corr/corr_core.c     1-D FFT correlator + int-dump
#   native/src/corr2d/corr2d_core.c 2-D FFT correlator + int-dump
#
# NOTE: the `ref` array parameter is not expressible as a just-makeit
# init_param (arrays are not supported).  The generated corr_create stub
# must be hand-edited to accept (const float complex *ref, size_t n, ...).
# The Python binding in spectral_ext.c takes ref as a numpy array and
# infers n (and ny/nx for Corr2D) from its shape.
#
# Generated files (~same pattern as FFT/FFT2D):
#   native/inc/corr/corr_core.h
#   native/src/corr/corr_core.c
#   native/inc/corr2d/corr2d_core.h
#   native/src/corr2d/corr2d_core.c
#   native/src/spectral/CMakeLists.txt    (updated — add corr/corr2d deps)
#   native/tests/test_corr_core.c
#   native/tests/test_corr2d_core.c
#   src/doppler/spectral/__init__.py      (updated — export Corr, Corr2D)
#   src/doppler/spectral/spectral.pyi     (updated — stubs for Corr, Corr2D)
#   CMakeLists.txt                        (updated — add_subdirectory)
#   just-makeit.toml                      (updated)
#
# Hand-edit (after scaffold):
#   native/inc/corr/corr_core.h
#     — struct: fft_state_t *fwd, *inv; float complex *ref_spec,
#       *work_fft, *work_ifft, *accum; size_t n, dwell, count;
#     — corr_create(const float complex *ref, size_t n, size_t dwell,
#                   int nthreads)
#     — corr_set_ref(state, ref) — recompute ref spectrum + reset
#
#   native/src/corr/corr_core.c
#     — corr_create: alloc fwd/inv plans, calloc accum, pre-compute
#       ref_spec = conj(FFT(ref))
#     — corr_execute: FFT(in) → work_fft; work_fft *= ref_spec;
#       IFFT → work_ifft; accum += work_ifft / n; dump on count==dwell
#     — corr_reset: zero accum + count = 0
#
#   native/inc/corr2d/corr2d_core.h  (mirror of corr_core.h for 2-D)
#   native/src/corr2d/corr2d_core.c  (uses fft2d_* instead of fft_*)
#
#   native/src/spectral/spectral_ext.c
#     — add CorrObject and Corr2DObject following the FFT/FFT2D pattern
#     — Corr.__init__ accepts (ref: ndarray, dwell=1, nthreads=1)
#       and infers n = len(ref)
#     — Corr2D.__init__ requires ref.ndim == 2; infers ny, nx from shape
#     — execute() returns None on no-dump, CF32 ndarray view on dump
#     — corr_create() note: just-makeit stub uses scalar n; replace with
#       array ref parameter
#
set -euo pipefail

JM="${JUST_MAKEIT:-just-makeit}"

# ── Corr object (1-D FFT correlator + int-dump) ───────────────────────────────
#
# --no-state : hand-edit struct (fwd/inv plans + 4 scratch buffers)
# --no-step  : no single-sample step; processing via execute()
# init_params n/dwell/nthreads are placeholders; actual C API has ref array.
$JM object corr \
    --module spectral \
    --class-name Corr \
    --no-state \
    --no-step \
    --init-param "n:size_t:1024" \
    --init-param "dwell:size_t:1" \
    --init-param "nthreads:int:1"

# execute: input CF32 frame → optional CF32 correlation output.
# Returns n on dump, 0 otherwise; Python binding translates to None.
$JM method corr execute \
    --module spectral \
    --arg-type "float _Complex" \
    --return-type "float _Complex" \
    --variable-output

$JM property corr n \
    --module spectral \
    --type size_t \
    --field

$JM property corr dwell \
    --module spectral \
    --type size_t \
    --field

$JM property corr count \
    --module spectral \
    --type size_t \
    --field

# ── Corr2D object (2-D FFT correlator + int-dump) ─────────────────────────────
$JM object corr2d \
    --module spectral \
    --class-name Corr2D \
    --no-state \
    --no-step \
    --init-param "ny:size_t:64" \
    --init-param "nx:size_t:64" \
    --init-param "dwell:size_t:1" \
    --init-param "nthreads:int:1"

$JM method corr2d execute \
    --module spectral \
    --arg-type "float _Complex" \
    --return-type "float _Complex" \
    --variable-output

$JM property corr2d ny \
    --module spectral \
    --type size_t \
    --field

$JM property corr2d nx \
    --module spectral \
    --type size_t \
    --field

$JM property corr2d dwell \
    --module spectral \
    --type size_t \
    --field

$JM property corr2d count \
    --module spectral \
    --type size_t \
    --field

echo
echo "corr scaffold done. Hand-edit the files listed above."
