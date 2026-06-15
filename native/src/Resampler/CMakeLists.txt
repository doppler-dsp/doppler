# OBJECT library — pure C core, no Python dependency.
add_library(Resampler_core OBJECT Resampler_core.c)
target_include_directories(
  Resampler_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/Resampler)
target_link_libraries(Resampler_core PUBLIC
    resamp_core)

add_executable(test_Resampler_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_Resampler_core.c)
target_link_libraries(test_Resampler_core
                      PRIVATE Resampler_core resamp_core
    m)
target_include_directories(test_Resampler_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_Resampler_core COMMAND test_Resampler_core)

add_executable(
  bench_Resampler_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_Resampler_core.c)
target_link_libraries(bench_Resampler_core
                      PRIVATE Resampler_core resamp_core
    m)
target_include_directories(
  bench_Resampler_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
