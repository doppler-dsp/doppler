add_library(dsss_core OBJECT dsss_core.c bin_to_signed.c)
target_include_directories(dsss_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# dsss Python module — aggregates: Despreader, BurstDespreader, Acquisition, BurstAcquisition, PolynomialPhaseEstimator, BurstDemod, DsssReceiver, AsyncDsssReceiver, DsssBurstReceiver
Python3_add_library(dsss MODULE WITH_SOABI dsss_ext.c)
target_link_libraries(dsss PRIVATE
    dsss_core
    despreader_core
    burst_despreader_core
    acq_core
    burst_acq_core
    ppe_core
    burst_demod_core
    dsss_receiver_core
    async_dsss_receiver_core
    dsss_burst_receiver_core
    costas_core
    lo_core
    lockdet_core
    loop_filter_core
    dp_tlm_core
    dll_core
    detection_core
    corr2d_core
    fft2d_core
    fft_core
    spectral_core
    wfm_frame_core
    ccsds_tm_core
    conv_core
    rs_core
    pn_core
    gold_core
    mpsk_core
    wfm_dsp_core
    RateConverter_core
    resamp_core
    fir_core
    agc_core
    mpsk_receiver_core
    ddc_core
    hbdecim_core
    hbdecim_r2c_core
    cic_core
    resample_core
    ddcr_core
    ratesync_core
    symsync_core
    nco_core
    farrow_core
    snr_core
    carrier_acq_core
    psd_core
    acc_trace_core
    detector_core
    corr_core
    Python3::NumPy)
target_include_directories(dsss PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(dsss PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/dsss"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/dsss")
add_custom_command(TARGET dsss POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:dsss>"
        "${PYTHON_PACKAGE_DIR}/dsss/$<TARGET_FILE_NAME:dsss>"
    VERBATIM
    COMMENT "Copy dsss extension module")
endif()

add_executable(test_dsss_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_dsss_core.c)
target_link_libraries(test_dsss_core PRIVATE
    dsss_core
    despreader_core
    burst_despreader_core
    acq_core
    burst_acq_core
    ppe_core
    burst_demod_core
    dsss_receiver_core
    async_dsss_receiver_core
    dsss_burst_receiver_core
    costas_core
    lo_core
    lockdet_core
    loop_filter_core
    dp_tlm_core
    dll_core
    detection_core
    corr2d_core
    fft2d_core
    fft_core
    spectral_core
    wfm_frame_core
    ccsds_tm_core
    conv_core
    rs_core
    pn_core
    gold_core
    mpsk_core
    wfm_dsp_core
    RateConverter_core
    resamp_core
    fir_core
    agc_core
    mpsk_receiver_core
    ddc_core
    hbdecim_core
    hbdecim_r2c_core
    cic_core
    resample_core
    ddcr_core
    ratesync_core
    symsync_core
    nco_core
    farrow_core
    snr_core
    carrier_acq_core
    psd_core
    acc_trace_core
    detector_core
    corr_core
    m)
target_include_directories(test_dsss_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_dsss_core COMMAND test_dsss_core)

add_executable(bench_dsss_core
    ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_dsss_core.c)
target_link_libraries(bench_dsss_core PRIVATE
    dsss_core
    despreader_core
    burst_despreader_core
    acq_core
    burst_acq_core
    ppe_core
    burst_demod_core
    dsss_receiver_core
    async_dsss_receiver_core
    dsss_burst_receiver_core
    costas_core
    lo_core
    lockdet_core
    loop_filter_core
    dp_tlm_core
    dll_core
    detection_core
    corr2d_core
    fft2d_core
    fft_core
    spectral_core
    wfm_frame_core
    ccsds_tm_core
    conv_core
    rs_core
    pn_core
    gold_core
    mpsk_core
    wfm_dsp_core
    RateConverter_core
    resamp_core
    fir_core
    agc_core
    mpsk_receiver_core
    ddc_core
    hbdecim_core
    hbdecim_r2c_core
    cic_core
    resample_core
    ddcr_core
    ratesync_core
    symsync_core
    nco_core
    farrow_core
    snr_core
    carrier_acq_core
    psd_core
    acc_trace_core
    detector_core
    corr_core
    m)
target_include_directories(bench_dsss_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
            ${CMAKE_SOURCE_DIR}/native/benchmarks)
