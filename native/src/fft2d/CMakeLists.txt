# OBJECT library — pure C core, no Python dependency.
add_library(fft2d_core OBJECT fft2d_core.c)
target_include_directories(
  fft2d_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/fft2d)
target_link_libraries(fft2d_core PUBLIC
    fft_core)

add_executable(test_fft2d_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_fft2d_core.c)
target_link_libraries(test_fft2d_core
                      PRIVATE fft2d_core fft_core
    m)
target_include_directories(test_fft2d_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_fft2d_core COMMAND test_fft2d_core)

add_executable(
  bench_fft2d_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_fft2d_core.c)
target_link_libraries(bench_fft2d_core
                      PRIVATE fft2d_core fft_core
    m)
target_include_directories(
  bench_fft2d_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
