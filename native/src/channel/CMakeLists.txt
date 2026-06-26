# OBJECT library — pure C core, no Python dependency.
add_library(channel_core OBJECT channel_core.c)
target_include_directories(
  channel_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/channel)
target_link_libraries(channel_core PUBLIC
    costas_core
    lo_core
    loop_filter_core
    dll_core)

add_executable(test_channel_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_channel_core.c)
target_link_libraries(test_channel_core
                      PRIVATE channel_core costas_core
    lo_core
    loop_filter_core
    dll_core
    m)
target_include_directories(test_channel_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_channel_core COMMAND test_channel_core)

add_executable(
  bench_channel_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_channel_core.c)
target_link_libraries(bench_channel_core
                      PRIVATE channel_core costas_core
    lo_core
    loop_filter_core
    dll_core
    m)
target_include_directories(
  bench_channel_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
