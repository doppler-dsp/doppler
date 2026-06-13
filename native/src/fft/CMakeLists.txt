# pocketfft is pure C99 (the upstream Reinecke core pocketfft_c99.c, double,
# libm-only) wrapped by pocketfft.c.  pffft.c is the vendored PFFFT (Pommier,
# BSD/FFTPACK) — a native single-precision SIMD FFT (SSE/NEON, scalar fallback)
# the wrapper routes cf32/integer-IQ through for 5-smooth sizes, falling back to
# pocketfft otherwise.  All compiled straight into fft_core so their symbols
# travel to every consumer (fft2d, corr, welch, ...).  Still C++-free, -lm only;
# PFFFT emits only 128-bit xmm/NEON, so the portable-SIMD gate stays green.
add_library(fft_core OBJECT fft_core.c pocketfft.c pocketfft_c99.c pffft.c)
target_include_directories(fft_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/fft)
# pffft.c does #include "pffft.h" and uses M_PI/M_SQRT2 (not in strict C99); give
# it the vendored header dir and the math-constant feature macro, TU-scoped so
# nothing else is affected and the vendored source stays pristine.
set_source_files_properties(pffft.c PROPERTIES
    INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/native/inc/pffft
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
