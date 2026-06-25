# OBJECT library — pure C core, no Python dependency.
add_library(loop_filter_core OBJECT loop_filter_core.c)
target_include_directories(
  loop_filter_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/loop_filter)

add_executable(test_loop_filter_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_loop_filter_core.c)
target_link_libraries(test_loop_filter_core
                      PRIVATE loop_filter_core m)
target_include_directories(test_loop_filter_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_loop_filter_core COMMAND test_loop_filter_core)

add_executable(
  bench_loop_filter_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_loop_filter_core.c)
target_link_libraries(bench_loop_filter_core
                      PRIVATE loop_filter_core m)
target_include_directories(
  bench_loop_filter_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
