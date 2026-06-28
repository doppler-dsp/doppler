# OBJECT library — pure C core, no Python dependency.
add_library(ppe_core OBJECT ppe_core.c)
target_include_directories(
  ppe_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/ppe)
target_link_libraries(ppe_core PUBLIC
    fft_core
    spectral_core)

add_executable(test_ppe_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_ppe_core.c)
target_link_libraries(test_ppe_core
                      PRIVATE ppe_core fft_core
    spectral_core
    m)
target_include_directories(test_ppe_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_ppe_core COMMAND test_ppe_core)

add_executable(
  bench_ppe_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_ppe_core.c)
target_link_libraries(bench_ppe_core
                      PRIVATE ppe_core fft_core
    spectral_core
    m)
target_include_directories(
  bench_ppe_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
