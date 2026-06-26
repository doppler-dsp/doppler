# OBJECT library — pure C core, no Python dependency.
add_library(farrow_core OBJECT farrow_core.c)
target_include_directories(
  farrow_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/farrow)

add_executable(test_farrow_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_farrow_core.c)
target_link_libraries(test_farrow_core
                      PRIVATE farrow_core m)
target_include_directories(test_farrow_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_farrow_core COMMAND test_farrow_core)

add_executable(
  bench_farrow_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_farrow_core.c)
target_link_libraries(bench_farrow_core
                      PRIVATE farrow_core m)
target_include_directories(
  bench_farrow_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
