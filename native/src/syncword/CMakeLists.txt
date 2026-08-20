# OBJECT library — pure C core, no Python dependency.
add_library(syncword_core OBJECT syncword_core.c)
target_include_directories(
  syncword_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/syncword)

add_executable(test_syncword_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_syncword_core.c)
target_link_libraries(test_syncword_core
                      PRIVATE syncword_core m)
target_include_directories(test_syncword_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_syncword_core COMMAND test_syncword_core)

add_executable(
  bench_syncword_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_syncword_core.c)
target_link_libraries(bench_syncword_core
                      PRIVATE syncword_core m)
target_include_directories(
  bench_syncword_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
