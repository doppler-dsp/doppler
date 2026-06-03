# OBJECT library — pure C core, no Python dependency.
add_library(acc_q8_core OBJECT acc_q8_core.c)
target_include_directories(
  acc_q8_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/acc_q8)

add_executable(test_acc_q8_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_acc_q8_core.c)
target_link_libraries(test_acc_q8_core
                      PRIVATE acc_q8_core m)
target_include_directories(test_acc_q8_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_acc_q8_core COMMAND test_acc_q8_core)

add_executable(
  bench_acc_q8_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_acc_q8_core.c)
target_link_libraries(bench_acc_q8_core
                      PRIVATE acc_q8_core m)
target_include_directories(
  bench_acc_q8_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
