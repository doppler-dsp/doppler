# OBJECT library — pure C core, no Python dependency.
add_library(tonemeas_core OBJECT tonemeas_core.c)
target_include_directories(
  tonemeas_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/tonemeas)
target_link_libraries(tonemeas_core PUBLIC
    fft_core
    spectral_core
    psd_core
    acc_trace_core)

add_executable(test_tonemeas_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_tonemeas_core.c)
target_link_libraries(test_tonemeas_core
                      PRIVATE tonemeas_core fft_core
    spectral_core
    psd_core
    acc_trace_core
    m)
target_include_directories(test_tonemeas_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_tonemeas_core COMMAND test_tonemeas_core)

add_executable(
  bench_tonemeas_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_tonemeas_core.c)
target_link_libraries(bench_tonemeas_core
                      PRIVATE tonemeas_core fft_core
    spectral_core
    psd_core
    acc_trace_core
    m)
target_include_directories(
  bench_tonemeas_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
