# OBJECT library — pure C core, no Python dependency.
add_library(hbdecim_q15_core OBJECT hbdecim_q15_core.c)
target_include_directories(
  hbdecim_q15_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/hbdecim_q15)

add_executable(test_hbdecim_q15_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_hbdecim_q15_core.c)
target_link_libraries(test_hbdecim_q15_core
                      PRIVATE hbdecim_q15_core m)
target_include_directories(test_hbdecim_q15_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_hbdecim_q15_core COMMAND test_hbdecim_q15_core)

add_executable(
  bench_hbdecim_q15_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_hbdecim_q15_core.c)
target_link_libraries(bench_hbdecim_q15_core
                      PRIVATE hbdecim_q15_core m)
target_include_directories(
  bench_hbdecim_q15_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
