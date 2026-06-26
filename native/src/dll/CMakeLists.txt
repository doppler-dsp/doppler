# OBJECT library — pure C core, no Python dependency.
add_library(dll_core OBJECT dll_core.c)
target_include_directories(
  dll_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/dll)
target_link_libraries(dll_core PUBLIC
    loop_filter_core)

add_executable(test_dll_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_dll_core.c)
target_link_libraries(test_dll_core
                      PRIVATE dll_core loop_filter_core
    m)
target_include_directories(test_dll_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_dll_core COMMAND test_dll_core)

add_executable(
  bench_dll_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_dll_core.c)
target_link_libraries(bench_dll_core
                      PRIVATE dll_core loop_filter_core
    m)
target_include_directories(
  bench_dll_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
