#!/usr/bin/env bash
# spectral_scaffold.sh — declare the spectral module interface via just-makeit.
#
# Run from: doppler-jm/   (project root, must contain just-makeit.toml)
#
# Reference algorithm (read-only — do not wrap):
#   ../native/src/fft/fft_core.c        1-D FFT via pocketfft
#   ../native/src/fft2d/fft2d_core.c    2-D FFT via pocketfft
#   ../native/src/spectral/spectral_core.c  kaiser / hann / magnitude_db / find_peaks
#
# Vendored pocketfft (already in place before running this script):
#   native/inc/pocketfft/pocketfft.h
#   native/src/spectral/pocketfft.cc
#
# Generated (~16 files — do not edit):
#   native/inc/fft/fft_core.h
#   native/src/fft/fft_core.c
#   native/inc/fft2d/fft2d_core.h
#   native/src/fft2d/fft2d_core.c
#   native/inc/spectral/spectral_core.h   (module umbrella)
#   native/src/spectral/spectral_core.c   (module-level stubs)
#   native/src/spectral/CMakeLists.txt
#   native/benchmarks/bench_fft_core.c
#   native/benchmarks/bench_fft2d_core.c
#   native/tests/test_fft_core.c
#   native/tests/test_fft2d_core.c
#   src/doppler_jm/spectral/__init__.py
#   src/doppler_jm/spectral/spectral.pyi
#   src/doppler_jm/spectral/tests/test_spectral.py
#   src/doppler_jm/spectral/benchmarks/bench_spectral.py
#   CMakeLists.txt                         (updated by jm)
#   just-makeit.toml                       (updated by jm)
#
# Hand-edit (7 files):
#   native/inc/fft/fft_core.h
#     — struct: pocketfft_plan *plan_f64, *plan_f32; size_t n; int sign;
#     — fft_create(size_t n, int sign, int nthreads)
#
#   native/src/fft/fft_core.c
#     — include pocketfft.h; implement lifecycle + execute_{cf64,cf32,inplace_*}
#
#   native/inc/fft2d/fft2d_core.h
#     — struct: pocketfft_plan *plan_f64, *plan_f32; size_t ny, nx; int sign;
#     — fft2d_create(size_t ny, size_t nx, int sign, int nthreads)
#
#   native/src/fft2d/fft2d_core.c
#     — implement lifecycle + execute_{cf64,cf32,inplace_*}; max_out = ny*nx
#
#   native/inc/spectral/spectral_core.h
#     — dp_peak_t struct; declare kaiser_enbw, kaiser_window, hann_window,
#       magnitude_db_cf32/cf64, find_peaks_f32
#
#   native/src/spectral/spectral_core.c
#     — implement all module-level functions (copy from reference)
#
#   native/src/spectral/CMakeLists.txt
#     — add CXX for pocketfft.cc; pocketfft_cxx static lib; link spectral.so
#
#   native/src/spectral/spectral_ext.c
#     — add dtype-dispatching execute/execute_inplace methods to FFT + FFT2D
#     — fix _bind_magnitude_db_cf32/cf64 to return float32 ndarray
#     — fix _bind_find_peaks_f32 to return list[tuple[float, float]]
#     — fix _bind_kaiser_window / _bind_hann_window to accept writable array
#
#   native/tests/test_fft_core.c
#     — lifecycle, forward/inverse round-trip CF64 and CF32, inplace
#
#   native/tests/test_fft2d_core.c (if generated separately)
#
#   src/doppler_jm/spectral/tests/test_spectral.py
#     — mirror C tests at Python layer; dtype dispatch; find_peaks; kaiser
#
set -euo pipefail

JM="${JUST_MAKEIT:-just-makeit}"

# ── Module ────────────────────────────────────────────────────────────────────
$JM module spectral

# ── FFT object (1-D FFT plan, dual CF64+CF32) ─────────────────────────────────
#
# --no-state : 4 fields (plan_f64*, plan_f32*, n, sign); jm cannot infer
# --no-step  : no single-sample step; all processing via execute_* methods
# Three init params: n (transform length), sign (±1), nthreads (ignored)
$JM object fft \
    --module spectral \
    --class-name FFT \
    --no-state \
    --no-step \
    --init-param "n:size_t:1024" \
    --init-param "sign:int:-1" \
    --init-param "nthreads:int:1"

# Out-of-place CF64 execute: input CF64 array → output CF64 array (always n).
$JM method fft execute_cf64 \
    --module spectral \
    --arg-type "double _Complex" \
    --return-type "double _Complex" \
    --variable-output

# Out-of-place CF32 execute: input CF32 array → output CF32 array.
$JM method fft execute_cf32 \
    --module spectral \
    --arg-type "float _Complex" \
    --return-type "float _Complex" \
    --variable-output

# In-place CF64: copies in→out then transforms; output always n.
$JM method fft execute_inplace_cf64 \
    --module spectral \
    --arg-type "double _Complex" \
    --return-type "double _Complex" \
    --variable-output

# In-place CF32: copies in→out then transforms.
$JM method fft execute_inplace_cf32 \
    --module spectral \
    --arg-type "float _Complex" \
    --return-type "float _Complex" \
    --variable-output

# n: transform length (read-only field).
$JM property fft n \
    --module spectral \
    --type size_t \
    --field

# sign: -1 forward, +1 inverse (read-only field).
$JM property fft sign \
    --module spectral \
    --type int \
    --field

# ── FFT2D object (2-D FFT plan, dual CF64+CF32) ───────────────────────────────
#
# --no-state : 5 fields (plan_f64*, plan_f32*, ny, nx, sign)
# --no-step  : no single-sample step
$JM object fft2d \
    --module spectral \
    --class-name FFT2D \
    --no-state \
    --no-step \
    --init-param "ny:size_t:64" \
    --init-param "nx:size_t:64" \
    --init-param "sign:int:-1" \
    --init-param "nthreads:int:1"

# Execute methods mirror FFT; max_out = ny*nx (hand-edit).
$JM method fft2d execute_cf64 \
    --module spectral \
    --arg-type "double _Complex" \
    --return-type "double _Complex" \
    --variable-output

$JM method fft2d execute_cf32 \
    --module spectral \
    --arg-type "float _Complex" \
    --return-type "float _Complex" \
    --variable-output

$JM method fft2d execute_inplace_cf64 \
    --module spectral \
    --arg-type "double _Complex" \
    --return-type "double _Complex" \
    --variable-output

$JM method fft2d execute_inplace_cf32 \
    --module spectral \
    --arg-type "float _Complex" \
    --return-type "float _Complex" \
    --variable-output

$JM property fft2d ny \
    --module spectral \
    --type size_t \
    --field

$JM property fft2d nx \
    --module spectral \
    --type size_t \
    --field

$JM property fft2d sign \
    --module spectral \
    --type int \
    --field

# ── Module-level functions ────────────────────────────────────────────────────

# kaiser_enbw: scalar float return; hand-edit if needed (jm generates correct binding).
$JM function kaiser_enbw \
    --module spectral \
    --param "w:float[]" \
    --return-type float

# kaiser_window: fills w in-place; jm generates const float* — hand-edit to float*.
$JM function kaiser_window \
    --module spectral \
    --param "w:float[]" \
    --param "beta:float" \
    --return-type void

# hann_window: fills w in-place; same hand-edit needed as kaiser_window.
$JM function hann_window \
    --module spectral \
    --param "w:float[]" \
    --return-type void

# magnitude_db_cf32: returns float32 ndarray; hand-edit _bind_ in ext.c.
$JM function magnitude_db_cf32 \
    --module spectral \
    --param "in:float _Complex[]" \
    --param "lin_floor:float" \
    --param "offset_db:float" \
    --return-type void

# magnitude_db_cf64: returns float32 ndarray; hand-edit _bind_ in ext.c.
$JM function magnitude_db_cf64 \
    --module spectral \
    --param "in:double _Complex[]" \
    --param "lin_floor:double" \
    --param "offset_db:float" \
    --return-type void

# find_peaks_f32: returns list[tuple[float,float]]; hand-edit _bind_ in ext.c.
$JM function find_peaks_f32 \
    --module spectral \
    --param "db:float[]" \
    --param "n_peaks:size_t" \
    --param "min_db:float" \
    --return-type void

echo
echo "spectral scaffold done. Hand-edit the files listed above."
