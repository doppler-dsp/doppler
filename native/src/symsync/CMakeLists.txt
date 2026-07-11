# OBJECT library — pure C core, no Python dependency.
add_library(symsync_core OBJECT symsync_core.c)
target_include_directories(
  symsync_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/symsync)
target_link_libraries(symsync_core PUBLIC
    nco_core
    farrow_core
    loop_filter_core
    lockdet_core
    telemetry_core)

add_executable(test_symsync_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_symsync_core.c)
target_link_libraries(test_symsync_core
                      PRIVATE symsync_core nco_core
    farrow_core
    loop_filter_core
    lockdet_core
    telemetry_core
    m)
target_include_directories(test_symsync_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_symsync_core COMMAND test_symsync_core)

add_executable(
  bench_symsync_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_symsync_core.c)
target_link_libraries(bench_symsync_core
                      PRIVATE symsync_core nco_core
    farrow_core
    loop_filter_core
    lockdet_core
    telemetry_core
    m)
target_include_directories(
  bench_symsync_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
