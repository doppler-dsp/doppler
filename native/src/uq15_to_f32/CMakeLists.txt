# OBJECT library — pure C core, no Python dependency.
add_library(uq15_to_f32_core OBJECT uq15_to_f32_core.c)
target_include_directories(
  uq15_to_f32_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/uq15_to_f32)

add_executable(test_uq15_to_f32_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_uq15_to_f32_core.c)
target_link_libraries(test_uq15_to_f32_core
                      PRIVATE uq15_to_f32_core m)
target_include_directories(test_uq15_to_f32_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_uq15_to_f32_core COMMAND test_uq15_to_f32_core)

add_executable(
  bench_uq15_to_f32_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_uq15_to_f32_core.c)
target_link_libraries(bench_uq15_to_f32_core
                      PRIVATE uq15_to_f32_core m)
target_include_directories(
  bench_uq15_to_f32_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
