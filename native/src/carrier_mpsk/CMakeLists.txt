# OBJECT library — pure C core, no Python dependency.
add_library(carrier_mpsk_core OBJECT carrier_mpsk_core.c)
target_include_directories(
  carrier_mpsk_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/carrier_mpsk)
target_link_libraries(carrier_mpsk_core PUBLIC
    lo_core
    loop_filter_core)

add_executable(test_carrier_mpsk_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_carrier_mpsk_core.c)
target_link_libraries(test_carrier_mpsk_core
                      PRIVATE carrier_mpsk_core lo_core
    loop_filter_core
    m)
target_include_directories(test_carrier_mpsk_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_carrier_mpsk_core COMMAND test_carrier_mpsk_core)

add_executable(
  bench_carrier_mpsk_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_carrier_mpsk_core.c)
target_link_libraries(bench_carrier_mpsk_core
                      PRIVATE carrier_mpsk_core lo_core
    loop_filter_core
    m)
target_include_directories(
  bench_carrier_mpsk_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
