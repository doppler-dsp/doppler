# OBJECT library — pure C core, no Python dependency.
add_library(dsss_burst_receiver_core OBJECT dsss_burst_receiver_core.c)
target_include_directories(
  dsss_burst_receiver_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/dsss_burst_receiver)
target_link_libraries(dsss_burst_receiver_core PUBLIC
    burst_acq_core
    acq_core
    corr2d_core
    fft2d_core
    fft_core
    detection_core
    burst_demod_core
    ppe_core
    spectral_core
    wfm_frame_core
    ccsds_tm_core
    conv_core
    rs_core
    pn_core
    gold_core
    mpsk_core
    burst_despreader_core
    loop_filter_core
    corr_core)

add_executable(test_dsss_burst_receiver_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_dsss_burst_receiver_core.c)
target_link_libraries(test_dsss_burst_receiver_core
                      PRIVATE dsss_burst_receiver_core burst_acq_core
    acq_core
    corr2d_core
    fft2d_core
    fft_core
    detection_core
    burst_demod_core
    ppe_core
    spectral_core
    wfm_frame_core
    ccsds_tm_core
    conv_core
    rs_core
    pn_core
    gold_core
    mpsk_core
    burst_despreader_core
    loop_filter_core
    corr_core
    m)
target_include_directories(test_dsss_burst_receiver_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_dsss_burst_receiver_core COMMAND test_dsss_burst_receiver_core)

add_executable(
  bench_dsss_burst_receiver_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_dsss_burst_receiver_core.c)
target_link_libraries(bench_dsss_burst_receiver_core
                      PRIVATE dsss_burst_receiver_core burst_acq_core
    acq_core
    corr2d_core
    fft2d_core
    fft_core
    detection_core
    burst_demod_core
    ppe_core
    spectral_core
    wfm_frame_core
    ccsds_tm_core
    conv_core
    rs_core
    pn_core
    gold_core
    mpsk_core
    burst_despreader_core
    loop_filter_core
    corr_core
    m)
target_include_directories(
  bench_dsss_burst_receiver_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
