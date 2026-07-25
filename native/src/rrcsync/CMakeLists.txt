# OBJECT library — pure C core, no Python dependency.
add_library(rrcsync_core OBJECT rrcsync_core.c)
target_include_directories(
  rrcsync_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/rrcsync)
target_link_libraries(rrcsync_core PUBLIC
    resamp_core
    loop_filter_core
    lockdet_core
    telemetry_core)

add_executable(test_rrcsync_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_rrcsync_core.c)
target_link_libraries(test_rrcsync_core
                      PRIVATE rrcsync_core resamp_core
    loop_filter_core
    lockdet_core
    telemetry_core
    m)
target_include_directories(test_rrcsync_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_rrcsync_core COMMAND test_rrcsync_core)

add_executable(
  bench_rrcsync_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_rrcsync_core.c)
target_link_libraries(bench_rrcsync_core
                      PRIVATE rrcsync_core resamp_core
    loop_filter_core
    lockdet_core
    telemetry_core
    m)
target_include_directories(
  bench_rrcsync_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
