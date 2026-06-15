# OBJECT library — pure C core, no Python dependency.
add_library(acc_cf64_core OBJECT acc_cf64_core.c)
target_include_directories(
  acc_cf64_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/acc_cf64)

add_executable(test_acc_cf64_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_acc_cf64_core.c)
target_link_libraries(test_acc_cf64_core
                      PRIVATE acc_cf64_core m)
target_include_directories(test_acc_cf64_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_acc_cf64_core COMMAND test_acc_cf64_core)

add_executable(
  bench_acc_cf64_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_acc_cf64_core.c)
target_link_libraries(bench_acc_cf64_core
                      PRIVATE acc_cf64_core m)
target_include_directories(
  bench_acc_cf64_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
