# OBJECT library — pure C core, no Python dependency.
add_library(psd_core OBJECT psd_core.c)
target_include_directories(
  psd_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/psd)
target_link_libraries(psd_core PUBLIC
    fft_core
    spectral_core
    acc_trace_core)

add_executable(test_psd_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_psd_core.c)
target_link_libraries(test_psd_core
                      PRIVATE psd_core fft_core
    spectral_core
    acc_trace_core
    m)
target_include_directories(test_psd_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_psd_core COMMAND test_psd_core)

add_executable(
  bench_psd_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_psd_core.c)
target_link_libraries(bench_psd_core
                      PRIVATE psd_core fft_core
    spectral_core
    acc_trace_core
    m)
target_include_directories(
  bench_psd_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
