# OBJECT library — pure C core, no Python dependency.
add_library(nco_core OBJECT nco_core.c)
target_include_directories(
  nco_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/nco)

add_executable(test_nco_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_nco_core.c)
target_link_libraries(test_nco_core
                      PRIVATE nco_core m)
target_include_directories(test_nco_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_nco_core COMMAND test_nco_core)

add_executable(
  bench_nco_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_nco_core.c)
target_link_libraries(bench_nco_core
                      PRIVATE nco_core m)
target_include_directories(
  bench_nco_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
