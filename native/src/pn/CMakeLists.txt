# OBJECT library — pure C core, no Python dependency.
add_library(pn_core OBJECT pn_core.c)
target_include_directories(
  pn_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/pn)

add_executable(test_pn_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_pn_core.c)
target_link_libraries(test_pn_core
                      PRIVATE pn_core m)
target_include_directories(test_pn_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_pn_core COMMAND test_pn_core)

add_executable(
  bench_pn_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_pn_core.c)
target_link_libraries(bench_pn_core
                      PRIVATE pn_core m)
target_include_directories(
  bench_pn_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
