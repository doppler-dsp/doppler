# OBJECT library — pure C core, no Python dependency.
add_library(burst_despreader_core OBJECT burst_despreader_core.c)
target_include_directories(
  burst_despreader_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/burst_despreader)
target_link_libraries(burst_despreader_core PUBLIC
    loop_filter_core)

add_executable(test_burst_despreader_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_burst_despreader_core.c)
target_link_libraries(test_burst_despreader_core
                      PRIVATE burst_despreader_core loop_filter_core
    m)
target_include_directories(test_burst_despreader_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_burst_despreader_core COMMAND test_burst_despreader_core)

add_executable(
  bench_burst_despreader_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_burst_despreader_core.c)
target_link_libraries(bench_burst_despreader_core
                      PRIVATE burst_despreader_core loop_filter_core
    m)
target_include_directories(
  bench_burst_despreader_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
