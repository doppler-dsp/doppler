# OBJECT library — pure C core, no Python dependency.
add_library(adc_core OBJECT adc_core.c)
target_include_directories(
  adc_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/adc)

add_executable(test_adc_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_adc_core.c)
target_link_libraries(test_adc_core
                      PRIVATE adc_core m)
target_include_directories(test_adc_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_adc_core COMMAND test_adc_core)

add_executable(
  bench_adc_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_adc_core.c)
target_link_libraries(bench_adc_core
                      PRIVATE adc_core m)
target_include_directories(
  bench_adc_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
