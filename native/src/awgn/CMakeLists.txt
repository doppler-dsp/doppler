# OBJECT library — pure C core, no Python dependency.
add_library(awgn_core OBJECT awgn_core.c)
target_include_directories(
  awgn_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/awgn)

add_executable(test_awgn_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_awgn_core.c)
target_link_libraries(test_awgn_core
                      PRIVATE awgn_core m)
target_include_directories(test_awgn_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_awgn_core COMMAND test_awgn_core)

add_executable(
  bench_awgn_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_awgn_core.c)
target_link_libraries(bench_awgn_core
                      PRIVATE awgn_core m)
target_include_directories(
  bench_awgn_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
