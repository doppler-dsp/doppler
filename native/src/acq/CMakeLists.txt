# OBJECT library — pure C core, no Python dependency.
add_library(acq_core OBJECT acq_core.c)
target_include_directories(
  acq_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/acq)
target_link_libraries(acq_core PUBLIC
    corr2d_core
    fft2d_core
    fft_core
    detection_core)

add_executable(test_acq_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_acq_core.c)
target_link_libraries(test_acq_core
                      PRIVATE acq_core corr2d_core
    fft2d_core
    fft_core
    detection_core
    m)
target_include_directories(test_acq_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_acq_core COMMAND test_acq_core)

add_executable(
  bench_acq_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_acq_core.c)
target_link_libraries(bench_acq_core
                      PRIVATE acq_core corr2d_core
    fft2d_core
    fft_core
    detection_core
    m)
target_include_directories(
  bench_acq_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
