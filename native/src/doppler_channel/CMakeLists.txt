# OBJECT library — pure C core, no Python dependency.
add_library(doppler_channel_core OBJECT doppler_channel_core.c)
target_include_directories(
  doppler_channel_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/doppler_channel)
target_link_libraries(doppler_channel_core PUBLIC
    resamp_core)

add_executable(test_doppler_channel_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_doppler_channel_core.c)
target_link_libraries(test_doppler_channel_core
                      PRIVATE doppler_channel_core resamp_core
    m)
target_include_directories(test_doppler_channel_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_doppler_channel_core COMMAND test_doppler_channel_core)

add_executable(
  bench_doppler_channel_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_doppler_channel_core.c)
target_link_libraries(bench_doppler_channel_core
                      PRIVATE doppler_channel_core resamp_core
    m)
target_include_directories(
  bench_doppler_channel_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
