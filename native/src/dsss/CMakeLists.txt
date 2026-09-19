# gh-1305: resolve libm to a PATH, not the bare name `m`. A bare `m` in
# `target_link_libraries` is looked up as a CMake TARGET first, so a module
# named `m` (`jm module m`) shadows the system math library: the object's
# test/bench link the module's DSO and fail with `undefined reference to sqrt`,
# and the module's own pair fail configure outright with "Target "m" of type
# MODULE_LIBRARY may not be linked into another target". A `find_library`
# result is an absolute path that no target name can shadow. Empty where libm
# lives in libc (MSVC, and platforms that fold it into libSystem), which links
# nothing.
find_library(JM_MATH_LIBRARY m)
if(NOT JM_MATH_LIBRARY)
  set(JM_MATH_LIBRARY "")
endif()

add_library(dsss_core OBJECT dsss_core.c bin_to_signed.c)
target_include_directories(dsss_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# dsss Python module — aggregates: Despreader, BurstDespreader, Acquisition, BurstAcquisition, PolynomialPhaseEstimator, BurstDemod, BurstCapture, DsssReceiver, AsyncDsssReceiver, AsyncDsssPool, DsssBurstReceiver, PersistentBurstCapture, CellAsyncDsssReceiver
Python3_add_library(dsss MODULE WITH_SOABI dsss_ext.c)
target_link_libraries(dsss PRIVATE
    dsss_core
    despreader_core
    burst_despreader_core
    acq_core
    burst_acq_core
    ppe_core
    burst_demod_core
    burst_capture_core
    dsss_receiver_core
    async_dsss_receiver_core
    async_dsss_pool_core
    dsss_burst_receiver_core
    costas_core
    lo_core
    lockdet_core
    loop_filter_core
    dp_tlm_core
    dll_core
    detection_core
    Threads::Threads
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
    dp_event_log_core
    wfm_writer_core
    wfm_cjson_core
    wfm_keywords_core
    wfm_draw_core
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
    burst_capture_core
    dsss_receiver_core
    async_dsss_receiver_core
    async_dsss_pool_core
    dsss_burst_receiver_core
    costas_core
    lo_core
    lockdet_core
    loop_filter_core
    dp_tlm_core
    dll_core
    detection_core
    Threads::Threads
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
    dp_event_log_core
    wfm_writer_core
    wfm_cjson_core
    wfm_keywords_core
    wfm_draw_core
    ${JM_MATH_LIBRARY})
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
    burst_capture_core
    dsss_receiver_core
    async_dsss_receiver_core
    async_dsss_pool_core
    dsss_burst_receiver_core
    costas_core
    lo_core
    lockdet_core
    loop_filter_core
    dp_tlm_core
    dll_core
    detection_core
    Threads::Threads
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
    dp_event_log_core
    wfm_writer_core
    wfm_cjson_core
    wfm_keywords_core
    wfm_draw_core
    ${JM_MATH_LIBRARY})
target_include_directories(bench_dsss_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
            ${CMAKE_SOURCE_DIR}/native/benchmarks)

# Your own CMake for this directory goes in dsss_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/dsss_extra.cmake OPTIONAL)
