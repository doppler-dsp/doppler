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

# OBJECT library — pure C core, no Python dependency.
add_library(async_dsss_pool_core OBJECT async_dsss_pool_core.c)
target_include_directories(
  async_dsss_pool_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                              ${CMAKE_SOURCE_DIR}/native/inc/async_dsss_pool)
target_link_libraries(async_dsss_pool_core PUBLIC
    Threads::Threads
    async_dsss_receiver_core
    ber_core
    acq_core
    corr2d_core
    fft2d_core
    fft_core
    detection_core
    dp_tlm_core
    cvt_core
    carrier_acq_core
    dll_core
    loop_filter_core
    lockdet_core
    costas_core
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    agc_core
    mpsk_receiver_core
    ddc_core
    hbdecim_r2c_core
    ddcr_core
    ratesync_core
    symsync_core
    nco_core
    farrow_core
    pn_core
    psd_core
    spectral_core
    acc_trace_core
    detector_core
    corr_core
    doppler_channel_core
    dp_event_log_core
    wfm_writer_core
    wfm_cjson_core
    wfm_keywords_core
    wfm_draw_core)

add_executable(test_async_dsss_pool_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_async_dsss_pool_core.c
               ${CMAKE_SOURCE_DIR}/native/tests/test_async_dsss_pool_symbols.c)
target_link_libraries(test_async_dsss_pool_core
                      PRIVATE async_dsss_pool_core Threads::Threads
    async_dsss_receiver_core
    ber_core
    acq_core
    corr2d_core
    fft2d_core
    fft_core
    detection_core
    dp_tlm_core
    cvt_core
    carrier_acq_core
    dll_core
    loop_filter_core
    lockdet_core
    costas_core
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    agc_core
    mpsk_receiver_core
    ddc_core
    hbdecim_r2c_core
    ddcr_core
    ratesync_core
    symsync_core
    nco_core
    farrow_core
    pn_core
    psd_core
    spectral_core
    acc_trace_core
    detector_core
    corr_core
    doppler_channel_core
    dp_event_log_core
    wfm_writer_core
    wfm_cjson_core
    wfm_keywords_core
    wfm_draw_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_async_dsss_pool_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_async_dsss_pool_core COMMAND test_async_dsss_pool_core)

add_executable(
  bench_async_dsss_pool_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_async_dsss_pool_core.c)
target_link_libraries(bench_async_dsss_pool_core
                      PRIVATE async_dsss_pool_core Threads::Threads
    async_dsss_receiver_core
    ber_core
    acq_core
    corr2d_core
    fft2d_core
    fft_core
    detection_core
    dp_tlm_core
    cvt_core
    carrier_acq_core
    dll_core
    loop_filter_core
    lockdet_core
    costas_core
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    agc_core
    mpsk_receiver_core
    ddc_core
    hbdecim_r2c_core
    ddcr_core
    ratesync_core
    symsync_core
    nco_core
    farrow_core
    pn_core
    psd_core
    spectral_core
    acc_trace_core
    detector_core
    corr_core
    doppler_channel_core
    dp_event_log_core
    wfm_writer_core
    wfm_cjson_core
    wfm_keywords_core
    wfm_draw_core
    ${JM_MATH_LIBRARY})
target_include_directories(
  bench_async_dsss_pool_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)

# Your own CMake for this directory goes in async_dsss_pool_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/async_dsss_pool_extra.cmake OPTIONAL)
