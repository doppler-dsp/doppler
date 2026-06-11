# OBJECT library — pure C core, no Python dependency.
add_library(welch_core OBJECT welch_core.c)
target_include_directories(
  welch_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/welch)
target_link_libraries(welch_core PUBLIC
    fft_core
    spectral_core
    acc_trace_core)

add_executable(test_welch_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_welch_core.c)
target_link_libraries(test_welch_core
                      PRIVATE welch_core fft_core
    spectral_core
    acc_trace_core
    m)
target_include_directories(test_welch_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_welch_core COMMAND test_welch_core)

add_executable(
  bench_welch_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_welch_core.c)
target_link_libraries(bench_welch_core
                      PRIVATE welch_core fft_core
    spectral_core
    acc_trace_core
    m)
target_include_directories(
  bench_welch_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
