# OBJECT library — pure C core, no Python dependency.
add_library(fir_core OBJECT fir_core.c)
target_include_directories(
  fir_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/fir)

add_executable(test_fir_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_fir_core.c)
target_link_libraries(test_fir_core
                      PRIVATE fir_core m)
target_include_directories(test_fir_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_fir_core COMMAND test_fir_core)

add_executable(
  bench_fir_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_fir_core.c)
target_link_libraries(bench_fir_core
                      PRIVATE fir_core m)
target_include_directories(
  bench_fir_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
