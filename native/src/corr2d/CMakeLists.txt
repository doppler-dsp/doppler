# OBJECT library — pure C core, no Python dependency.
add_library(corr2d_core OBJECT corr2d_core.c)
target_include_directories(
  corr2d_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/corr2d)
target_link_libraries(corr2d_core PUBLIC
    fft2d_core
    fft_core)

add_executable(test_corr2d_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_corr2d_core.c)
target_link_libraries(test_corr2d_core
                      PRIVATE corr2d_core fft2d_core
    fft_core
    m)
target_include_directories(test_corr2d_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_corr2d_core COMMAND test_corr2d_core)

add_executable(
  bench_corr2d_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_corr2d_core.c)
target_link_libraries(bench_corr2d_core
                      PRIVATE corr2d_core fft2d_core
    fft_core
    m)
target_include_directories(
  bench_corr2d_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
