# Hand-owned (jm includes this file and never writes it, gh-1351).
#
# awgn_core: the bench's searching row feeds the receiver the shipped noise
# generator, and nothing else here uses it. Linked onto the bench alone: a
# bare `depends_on` would also inject awgn's header into the receiver's
# public one and link it into the core and every composer of it.
target_link_libraries(bench_async_dsss_receiver_core PRIVATE awgn_core)

# An attach that cannot allocate is DP_ERR_MEMORY and changes nothing
# (#2846). The test compiles async_dsss_receiver_core.c INTO itself, so its
# link line is the core's dependency list without async_dsss_receiver_core,
# and it renames the two calls that can fail for that translation unit only:
# dp_tlm_create (the scratch context) and dp_acq_set_telemetry (the
# searcher's attach, which builds a scratch of its own). The counted
# stand-ins live in their own file, compiled with the renames undone, so
# they call the real ones. The pattern is test_acq_regrid_failure's.
add_executable(test_adr_telemetry_oom
    ${CMAKE_SOURCE_DIR}/native/tests/async_dsss_receiver/test_adr_telemetry_oom.c
    ${CMAKE_SOURCE_DIR}/native/tests/async_dsss_receiver/adr_tlm_hook.c)
target_compile_definitions(test_adr_telemetry_oom PRIVATE
    dp_tlm_create=adr_test_tlm_create
    dp_acq_set_telemetry=adr_test_acq_set_telemetry)
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/native/tests/async_dsss_receiver/adr_tlm_hook.c
    PROPERTIES COMPILE_OPTIONS "-Udp_tlm_create;-Udp_acq_set_telemetry")
target_link_libraries(test_adr_telemetry_oom PRIVATE
    wfm_dsp_core gold_core snr_core ber_core acq_core corr2d_core fft2d_core
    fft_core detection_core dp_tlm_core cvt_core carrier_acq_core psd_core
    spectral_core acc_trace_core detector_core corr_core dll_core
    loop_filter_core lockdet_core costas_core lo_core RateConverter_core
    resamp_core hbdecim_core cic_core fir_core resample_core agc_core
    mpsk_receiver_core ddc_core hbdecim_r2c_core ddcr_core ratesync_core
    symsync_core nco_core farrow_core pn_core doppler_channel_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_adr_telemetry_oom PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/doppler/async_dsss_receiver
    ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_adr_telemetry_oom COMMAND test_adr_telemetry_oom)
