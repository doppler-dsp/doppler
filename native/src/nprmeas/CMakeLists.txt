# OBJECT library — pure C core, no Python dependency.
add_library(nprmeas_core OBJECT nprmeas_core.c)
target_include_directories(
  nprmeas_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/nprmeas)
target_link_libraries(nprmeas_core PUBLIC
    fft_core
    spectral_core)

add_executable(test_nprmeas_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_nprmeas_core.c)
target_link_libraries(test_nprmeas_core
                      PRIVATE nprmeas_core fft_core
    spectral_core
    m)
target_include_directories(test_nprmeas_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_nprmeas_core COMMAND test_nprmeas_core)

add_executable(
  bench_nprmeas_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_nprmeas_core.c)
target_link_libraries(bench_nprmeas_core
                      PRIVATE nprmeas_core fft_core
    spectral_core
    m)
target_include_directories(
  bench_nprmeas_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
