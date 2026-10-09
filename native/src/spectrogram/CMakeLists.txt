# spectrogram — hand-owned c_dep: a stream of any-size chunks in, rows of
# nfft-bin spectra out, one every hop samples (docs/design/spectrogram.md).
#
# A c_dep rather than a jm object because it has no Python face yet, and jm
# cannot declare an object without generating one: the binding (#1894 slice
# 3b) waits on just-makeit#2152 (`out_cols`), and moves this component to an
# objects/spectrogram.toml then. Until it does, jm apply only emits the
# add_subdirectory for this dir (listed in [project].c_deps); this file is
# hand-maintained, and the root CMakeLists.txt wires the core into libdoppler.
#
# It composes, and re-implements none of: PSD's per-frame kernel (psd_core,
# and through it fft_core / spectral_core / acc_trace_core) and the ring's
# framed face (f32_buffer_core, header-only, which needs the interrupt guard's
# one definition).

add_library(spectrogram_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/spectrogram/spectrogram_core.c)
target_include_directories(spectrogram_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(spectrogram_core PUBLIC
    psd_core fft_core spectral_core acc_trace_core
    f32_buffer_core dp_interrupt_guard_core)

# An OBJECT library hands its own objects to what links it, not those of the
# OBJECT libraries it links in turn, so every executable names the whole chain.
add_executable(test_spectrogram_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_spectrogram_core.c)
target_link_libraries(test_spectrogram_core PRIVATE
    spectrogram_core psd_core fft_core spectral_core acc_trace_core
    f32_buffer_core dp_interrupt_guard_core ${DP_MATH_LIBRARY})
target_include_directories(test_spectrogram_core PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_spectrogram_core COMMAND test_spectrogram_core)

# The benchmark. Hand-registered because `spectrogram` is a c_dep and not one
# of jm's components, so jm generates no target for it.
add_executable(bench_spectrogram_core
    ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_spectrogram_core.c)
target_link_libraries(bench_spectrogram_core PRIVATE
    spectrogram_core psd_core fft_core spectral_core acc_trace_core
    f32_buffer_core dp_interrupt_guard_core ${DP_MATH_LIBRARY})
target_include_directories(bench_spectrogram_core PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/benchmarks)
