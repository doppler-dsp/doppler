# OBJECT library — pure C core, no Python dependency.
add_library(despreader_core OBJECT despreader_core.c)
target_include_directories(
  despreader_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/despreader)
target_link_libraries(despreader_core PUBLIC
    loop_filter_core)

add_executable(test_despreader_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_despreader_core.c)
target_link_libraries(test_despreader_core
                      PRIVATE despreader_core loop_filter_core
    m)
target_include_directories(test_despreader_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_despreader_core COMMAND test_despreader_core)

add_executable(
  bench_despreader_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_despreader_core.c)
target_link_libraries(bench_despreader_core
                      PRIVATE despreader_core loop_filter_core
    m)
target_include_directories(
  bench_despreader_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
