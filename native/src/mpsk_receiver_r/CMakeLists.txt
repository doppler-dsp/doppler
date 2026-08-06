# OBJECT library — pure C core, no Python dependency.
add_library(mpsk_receiver_r_core OBJECT mpsk_receiver_r_core.c)
target_include_directories(
  mpsk_receiver_r_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/mpsk_receiver_r)
target_link_libraries(mpsk_receiver_r_core PUBLIC
    wfm_dsp_core
    ber_core
    ddcr_core
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    hbdecim_r2c_core
    ddc_core
    mpsk_receiver_core
    ratesync_core
    loop_filter_core
    lockdet_core
    symsync_core
    nco_core
    farrow_core
    dp_tlm_core
    agc_core
    boxcar_core)

add_executable(test_mpsk_receiver_r_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_mpsk_receiver_r_core.c)
target_link_libraries(test_mpsk_receiver_r_core
                      PRIVATE mpsk_receiver_r_core wfm_dsp_core
    ber_core
    ddcr_core
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    hbdecim_r2c_core
    ddc_core
    mpsk_receiver_core
    ratesync_core
    loop_filter_core
    lockdet_core
    symsync_core
    nco_core
    farrow_core
    dp_tlm_core
    agc_core
    boxcar_core
    m)
target_include_directories(test_mpsk_receiver_r_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_mpsk_receiver_r_core COMMAND test_mpsk_receiver_r_core)

add_executable(
  bench_mpsk_receiver_r_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_mpsk_receiver_r_core.c)
target_link_libraries(bench_mpsk_receiver_r_core
                      PRIVATE mpsk_receiver_r_core wfm_dsp_core
    ber_core
    ddcr_core
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    hbdecim_r2c_core
    ddc_core
    mpsk_receiver_core
    ratesync_core
    loop_filter_core
    lockdet_core
    symsync_core
    nco_core
    farrow_core
    dp_tlm_core
    agc_core
    boxcar_core
    m)
target_include_directories(
  bench_mpsk_receiver_r_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
