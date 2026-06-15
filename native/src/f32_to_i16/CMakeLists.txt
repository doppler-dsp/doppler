# OBJECT library — pure C core, no Python dependency.
add_library(f32_to_i16_core OBJECT f32_to_i16_core.c)
target_include_directories(
  f32_to_i16_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/f32_to_i16)

add_executable(test_f32_to_i16_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_f32_to_i16_core.c)
target_link_libraries(test_f32_to_i16_core
                      PRIVATE f32_to_i16_core m)
target_include_directories(test_f32_to_i16_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_f32_to_i16_core COMMAND test_f32_to_i16_core)

add_executable(
  bench_f32_to_i16_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_f32_to_i16_core.c)
target_link_libraries(bench_f32_to_i16_core
                      PRIVATE f32_to_i16_core m)
target_include_directories(
  bench_f32_to_i16_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
