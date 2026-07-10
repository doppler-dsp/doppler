# OBJECT library — pure C core, no Python dependency.
add_library(costas_core OBJECT costas_core.c)
target_include_directories(
  costas_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/costas)
target_link_libraries(costas_core PUBLIC
    lo_core
    loop_filter_core
    telemetry_core)

add_executable(test_costas_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_costas_core.c)
target_link_libraries(test_costas_core
                      PRIVATE costas_core lo_core
    loop_filter_core
    telemetry_core
    m)
target_include_directories(test_costas_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_costas_core COMMAND test_costas_core)

add_executable(
  bench_costas_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_costas_core.c)
target_link_libraries(bench_costas_core
                      PRIVATE costas_core lo_core
    loop_filter_core
    telemetry_core
    m)
target_include_directories(
  bench_costas_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
