# The two upstream FFT cores are vendored under vendor/ (whole third-party
# sources, kept pristine): vendor/pocketfft/pocketfft_c99.c is the Reinecke
# pure-C99 core (double, libm-only) and vendor/pffft/pffft.c is Pommier's PFFFT
# (BSD/FFTPACK) — a native single-precision SIMD FFT (SSE/NEON, scalar
# fallback).  doppler's own pocketfft.c wraps both behind its small FFT API,
# routing cf32/integer-IQ through PFFFT for 5-smooth sizes and falling back to
# pocketfft otherwise.  All compiled straight into fft_core so their symbols
# travel to every consumer (fft2d, corr, psd, ...).  Still C++-free, -lm only;
# PFFFT emits only 128-bit xmm/NEON, so the portable-SIMD gate stays green.
add_library(fft_core OBJECT
    fft_core.c
    pocketfft.c
    ${CMAKE_SOURCE_DIR}/vendor/pocketfft/pocketfft_c99.c
    ${CMAKE_SOURCE_DIR}/vendor/pffft/pffft.c)
target_include_directories(fft_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/fft)
# The wrapper and vendored cores reference the vendored headers as
# "pffft/pffft.h" / "pocketfft/pocketfft_c99.h"; vendor/ is the root that
# resolves those prefixes (private — consumers see only the public fft_core.h).
target_include_directories(fft_core PRIVATE ${CMAKE_SOURCE_DIR}/vendor)
# pffft.c does #include "pffft.h" (unprefixed) and uses M_PI/M_SQRT2 (not in
# strict C99); give it the vendored header dir and the math-constant feature
# macro, TU-scoped so nothing else is affected and the vendored source stays
# pristine.
set_source_files_properties(${CMAKE_SOURCE_DIR}/vendor/pffft/pffft.c PROPERTIES
    INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/vendor/pffft
    COMPILE_DEFINITIONS "_USE_MATH_DEFINES;_DEFAULT_SOURCE")
target_link_libraries(fft_core PUBLIC m)

add_executable(test_fft_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_fft_core.c)
target_include_directories(test_fft_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(test_fft_core PRIVATE fft_core m)
add_test(NAME test_fft_core COMMAND test_fft_core)

add_executable(bench_fft_core
    ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_fft_core.c)
target_include_directories(bench_fft_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(bench_fft_core PRIVATE fft_core m)
