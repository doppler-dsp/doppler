# OBJECT library — pure C core, no Python dependency.
add_library(interp_table_core OBJECT interp_table_core.c)
target_include_directories(
  interp_table_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/interp_table)

add_executable(test_interp_table_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_interp_table_core.c)
target_link_libraries(test_interp_table_core
                      PRIVATE interp_table_core m)
target_include_directories(test_interp_table_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_interp_table_core COMMAND test_interp_table_core)

add_executable(
  bench_interp_table_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_interp_table_core.c)
target_link_libraries(bench_interp_table_core
                      PRIVATE interp_table_core m)
target_include_directories(
  bench_interp_table_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
