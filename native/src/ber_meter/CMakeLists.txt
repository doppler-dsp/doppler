# OBJECT library — pure C core, no Python dependency.
add_library(ber_meter_core OBJECT ber_meter_core.c)
target_include_directories(
  ber_meter_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/ber_meter)
target_link_libraries(ber_meter_core PUBLIC
    detection_core
    ber_core)

add_executable(test_ber_meter_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_ber_meter_core.c)
target_link_libraries(test_ber_meter_core
                      PRIVATE ber_meter_core detection_core
    ber_core
    m)
target_include_directories(test_ber_meter_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_ber_meter_core COMMAND test_ber_meter_core)

add_executable(
  bench_ber_meter_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_ber_meter_core.c)
target_link_libraries(bench_ber_meter_core
                      PRIVATE ber_meter_core detection_core
    ber_core
    m)
target_include_directories(
  bench_ber_meter_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
