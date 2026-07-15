# OBJECT library — pure C core, no Python dependency.
add_library(gold_core OBJECT gold_core.c)
target_include_directories(
  gold_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/gold)

add_executable(test_gold_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_gold_core.c)
target_link_libraries(test_gold_core
                      PRIVATE gold_core m)
target_include_directories(test_gold_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_gold_core COMMAND test_gold_core)

add_executable(
  bench_gold_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_gold_core.c)
target_link_libraries(bench_gold_core
                      PRIVATE gold_core m)
target_include_directories(
  bench_gold_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
