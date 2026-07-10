# OBJECT library — pure C core, no Python dependency.
add_library(lockdet_core OBJECT lockdet_core.c)
target_include_directories(
  lockdet_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/lockdet)

add_executable(test_lockdet_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_lockdet_core.c)
target_link_libraries(test_lockdet_core
                      PRIVATE lockdet_core m)
target_include_directories(test_lockdet_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_lockdet_core COMMAND test_lockdet_core)

add_executable(
  bench_lockdet_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_lockdet_core.c)
target_link_libraries(bench_lockdet_core
                      PRIVATE lockdet_core m)
target_include_directories(
  bench_lockdet_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
