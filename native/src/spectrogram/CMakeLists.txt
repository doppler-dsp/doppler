# spectrogram — a stream of any-size chunks in, rows of nfft-bin spectra out,
# one every hop samples (docs/design/spectrogram.md).
#
# A jm object of the spectral module (objects/spectrogram.toml): jm emits this
# dir's add_subdirectory and wires the core into libdoppler from the root
# CMakeLists.txt, and generates the Python binding. This file stays
# hand-written: it builds the core and its C test and bench (the example
# is native/examples/CMakeLists.txt's), and the composition below is why.
#
# It composes, and re-implements none of: PSD's per-frame kernel (psd_core,
# and through it fft_core / spectral_core / acc_trace_core) and the ring's
# framed face (f32_buffer_core, header-only, which needs the interrupt guard's
# one definition).

add_library(spectrogram_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/spectrogram/spectrogram_core.c)
target_include_directories(spectrogram_core
    PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(spectrogram_core PUBLIC
    psd_core fft_core spectral_core acc_trace_core
    f32_buffer_core dp_interrupt_guard_core)

# An OBJECT library hands its own objects to what links it, not those of the
# OBJECT libraries it links in turn, so every executable names the whole chain.
# The generated symbols table (test_spectrogram_symbols.c) names every function
# the Python binding calls; linking it makes an undefined one fail the build.
add_executable(test_spectrogram_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_spectrogram_core.c
    ${CMAKE_SOURCE_DIR}/native/tests/test_spectrogram_symbols.c)
target_link_libraries(test_spectrogram_core PRIVATE
    spectrogram_core psd_core fft_core spectral_core acc_trace_core
    f32_buffer_core dp_interrupt_guard_core ${DP_MATH_LIBRARY})
target_include_directories(test_spectrogram_core PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_spectrogram_core COMMAND test_spectrogram_core)

# The benchmark. Hand-registered because this file is hand-written (above),
# so jm generates no bench target for the object.
add_executable(bench_spectrogram_core
    ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_spectrogram_core.c)
target_link_libraries(bench_spectrogram_core PRIVATE
    spectrogram_core psd_core fft_core spectral_core acc_trace_core
    f32_buffer_core dp_interrupt_guard_core ${DP_MATH_LIBRARY})
target_include_directories(bench_spectrogram_core PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/benchmarks)
