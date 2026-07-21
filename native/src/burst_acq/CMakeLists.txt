# OBJECT library — pure C core, no Python dependency.
add_library(burst_acq_core OBJECT burst_acq_core.c)
target_include_directories(
  burst_acq_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/burst_acq)
target_link_libraries(burst_acq_core PUBLIC
    acq_core
    corr2d_core
    fft2d_core
    fft_core
    detection_core)

add_executable(test_burst_acq_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_burst_acq_core.c)
target_link_libraries(test_burst_acq_core
                      PRIVATE burst_acq_core acq_core
    corr2d_core
    fft2d_core
    fft_core
    detection_core
    m)
target_include_directories(test_burst_acq_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_burst_acq_core COMMAND test_burst_acq_core)

add_executable(
  bench_burst_acq_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_burst_acq_core.c)
target_link_libraries(bench_burst_acq_core
                      PRIVATE burst_acq_core acq_core
    corr2d_core
    fft2d_core
    fft_core
    detection_core
    m)
target_include_directories(
  bench_burst_acq_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
