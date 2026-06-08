# OBJECT library — pure C core, no Python dependency.
add_library(i8_to_f32_core OBJECT i8_to_f32_core.c)
target_include_directories(
  i8_to_f32_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/i8_to_f32)

add_executable(test_i8_to_f32_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_i8_to_f32_core.c)
target_link_libraries(test_i8_to_f32_core
                      PRIVATE i8_to_f32_core m)
target_include_directories(test_i8_to_f32_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_i8_to_f32_core COMMAND test_i8_to_f32_core)

add_executable(
  bench_i8_to_f32_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_i8_to_f32_core.c)
target_link_libraries(bench_i8_to_f32_core
                      PRIVATE i8_to_f32_core m)
target_include_directories(
  bench_i8_to_f32_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
