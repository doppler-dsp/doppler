# pocketfft is now pure C99: the upstream Reinecke core (pocketfft_c99.c,
# double-precision, libm-only) wrapped by pocketfft.c, which adds out-of-place
# execution, cf32 promote-to-double, and 2-D row/column passes.  Both are
# compiled straight into fft_core so the pocketfft symbols travel with it to
# every consumer (fft2d, corr, welch, ...) — exactly as before, but with no C++
# and no threads: doppler links only -lm.
add_library(fft_core OBJECT fft_core.c pocketfft.c pocketfft_c99.c)
target_include_directories(fft_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/fft)
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
