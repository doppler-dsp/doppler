# OBJECT library — pure C core, no Python dependency.
add_library(interleaver_core OBJECT interleaver_core.c)
target_include_directories(
  interleaver_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/interleaver)

add_executable(test_interleaver_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_interleaver_core.c)
target_link_libraries(test_interleaver_core
                      PRIVATE interleaver_core m)
target_include_directories(test_interleaver_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_interleaver_core COMMAND test_interleaver_core)

add_executable(
  bench_interleaver_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_interleaver_core.c)
target_link_libraries(bench_interleaver_core
                      PRIVATE interleaver_core m)
target_include_directories(
  bench_interleaver_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
