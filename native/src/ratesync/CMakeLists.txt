# OBJECT library — pure C core, no Python dependency.
add_library(ratesync_core OBJECT ratesync_core.c)
target_include_directories(
  ratesync_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/ratesync)
target_link_libraries(ratesync_core PUBLIC
    RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    loop_filter_core
    lockdet_core
    symsync_core
    nco_core
    farrow_core
    telemetry_core)

add_executable(test_ratesync_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_ratesync_core.c)
target_link_libraries(test_ratesync_core
                      PRIVATE ratesync_core RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    loop_filter_core
    lockdet_core
    symsync_core
    nco_core
    farrow_core
    telemetry_core
    m)
target_include_directories(test_ratesync_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_ratesync_core COMMAND test_ratesync_core)

add_executable(
  bench_ratesync_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_ratesync_core.c)
target_link_libraries(bench_ratesync_core
                      PRIVATE ratesync_core RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    loop_filter_core
    lockdet_core
    symsync_core
    nco_core
    farrow_core
    telemetry_core
    m)
target_include_directories(
  bench_ratesync_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
