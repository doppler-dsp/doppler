# OBJECT library — pure C core, no Python dependency.
add_library(mpsk_receiver_core OBJECT mpsk_receiver_core.c)
target_include_directories(
  mpsk_receiver_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/mpsk_receiver)
target_link_libraries(mpsk_receiver_core PUBLIC
    wfm_dsp_core
    carrier_nda_core
    lo_core
    loop_filter_core
    boxcar_core
    agc_core
    telemetry_core
    symsync_core
    nco_core
    farrow_core
    fir_core)

add_executable(test_mpsk_receiver_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_mpsk_receiver_core.c)
target_link_libraries(test_mpsk_receiver_core
                      PRIVATE mpsk_receiver_core wfm_dsp_core
    carrier_nda_core
    lo_core
    loop_filter_core
    boxcar_core
    agc_core
    telemetry_core
    symsync_core
    nco_core
    farrow_core
    fir_core
    m)
target_include_directories(test_mpsk_receiver_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_mpsk_receiver_core COMMAND test_mpsk_receiver_core)

add_executable(
  bench_mpsk_receiver_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_mpsk_receiver_core.c)
target_link_libraries(bench_mpsk_receiver_core
                      PRIVATE mpsk_receiver_core wfm_dsp_core
    carrier_nda_core
    lo_core
    loop_filter_core
    boxcar_core
    agc_core
    telemetry_core
    symsync_core
    nco_core
    farrow_core
    fir_core
    m)
target_include_directories(
  bench_mpsk_receiver_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
