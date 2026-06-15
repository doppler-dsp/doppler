# OBJECT library — pure C core, no Python dependency.
add_library(cic_core OBJECT cic_core.c)
target_include_directories(
  cic_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/cic)

add_executable(test_cic_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_cic_core.c)
target_link_libraries(test_cic_core
                      PRIVATE cic_core m)
target_include_directories(test_cic_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_cic_core COMMAND test_cic_core)

add_executable(
  bench_cic_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_cic_core.c)
target_link_libraries(bench_cic_core
                      PRIVATE cic_core m)
target_include_directories(
  bench_cic_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
