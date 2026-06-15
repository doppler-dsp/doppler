# OBJECT library — pure C core, no Python dependency.
add_library(specan_core OBJECT specan_core.c)
target_include_directories(
  specan_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/specan)
target_link_libraries(specan_core PUBLIC
    ddc_core
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    hbdecim_r2c_core
    cic_core
    fir_core
    resample_core
    psd_core
    acc_trace_core
    fft_core
    spectral_core)

add_executable(test_specan_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_specan_core.c)
target_link_libraries(test_specan_core
                      PRIVATE specan_core ddc_core
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    hbdecim_r2c_core
    cic_core
    fir_core
    resample_core
    psd_core
    acc_trace_core
    fft_core
    spectral_core
    m)
target_include_directories(test_specan_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_specan_core COMMAND test_specan_core)

add_executable(
  bench_specan_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_specan_core.c)
target_link_libraries(bench_specan_core
                      PRIVATE specan_core ddc_core
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    hbdecim_r2c_core
    cic_core
    fir_core
    resample_core
    psd_core
    acc_trace_core
    fft_core
    spectral_core
    m)
target_include_directories(
  bench_specan_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
