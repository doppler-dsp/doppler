# OBJECT library — pure C core, no Python dependency.
add_library(RateConverter_core OBJECT RateConverter_core.c)
target_include_directories(
  RateConverter_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/RateConverter)
target_link_libraries(RateConverter_core PUBLIC
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core)

add_executable(test_RateConverter_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_RateConverter_core.c)
target_link_libraries(test_RateConverter_core
                      PRIVATE RateConverter_core resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    m)
target_include_directories(test_RateConverter_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_RateConverter_core COMMAND test_RateConverter_core)

add_executable(
  bench_RateConverter_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_RateConverter_core.c)
target_link_libraries(bench_RateConverter_core
                      PRIVATE RateConverter_core resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    m)
target_include_directories(
  bench_RateConverter_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
