# OBJECT library — pure C core, no Python dependency.
add_library(burst_demod_core OBJECT burst_demod_core.c)
target_include_directories(
  burst_demod_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/burst_demod)
target_link_libraries(burst_demod_core PUBLIC
    ppe_core
    fft_core
    spectral_core)

add_executable(test_burst_demod_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_burst_demod_core.c)
target_link_libraries(test_burst_demod_core
                      PRIVATE burst_demod_core ppe_core
    fft_core
    spectral_core
    m)
target_include_directories(test_burst_demod_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_burst_demod_core COMMAND test_burst_demod_core)

add_executable(
  bench_burst_demod_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_burst_demod_core.c)
target_link_libraries(bench_burst_demod_core
                      PRIVATE burst_demod_core ppe_core
    fft_core
    spectral_core
    m)
target_include_directories(
  bench_burst_demod_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
