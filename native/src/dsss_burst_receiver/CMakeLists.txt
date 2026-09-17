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
add_library(dsss_burst_receiver_core OBJECT dsss_burst_receiver_core.c)
target_include_directories(
  dsss_burst_receiver_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                                  ${CMAKE_SOURCE_DIR}/native/inc/dsss_burst_receiver)
target_link_libraries(dsss_burst_receiver_core PUBLIC
    burst_capture_core
    burst_acq_core
    acq_core
    corr2d_core
    fft2d_core
    fft_core
    detection_core
    dp_tlm_core
    pn_core
    burst_demod_core
    ppe_core
    spectral_core
    wfm_frame_core
    ccsds_tm_core
    conv_core
    rs_core
    gold_core
    mpsk_core
    burst_despreader_core
    loop_filter_core
    corr_core)

add_executable(test_dsss_burst_receiver_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_dsss_burst_receiver_core.c)
target_link_libraries(test_dsss_burst_receiver_core
                      PRIVATE dsss_burst_receiver_core burst_capture_core
    burst_acq_core
    acq_core
    corr2d_core
    fft2d_core
    fft_core
    detection_core
    dp_tlm_core
    pn_core
    burst_demod_core
    ppe_core
    spectral_core
    wfm_frame_core
    ccsds_tm_core
    conv_core
    rs_core
    gold_core
    mpsk_core
    burst_despreader_core
    loop_filter_core
    corr_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_dsss_burst_receiver_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_dsss_burst_receiver_core COMMAND test_dsss_burst_receiver_core)

add_executable(
  bench_dsss_burst_receiver_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_dsss_burst_receiver_core.c)
target_link_libraries(bench_dsss_burst_receiver_core
                      PRIVATE dsss_burst_receiver_core burst_capture_core
    burst_acq_core
    acq_core
    corr2d_core
    fft2d_core
    fft_core
    detection_core
    dp_tlm_core
    pn_core
    burst_demod_core
    ppe_core
    spectral_core
    wfm_frame_core
    ccsds_tm_core
    conv_core
    rs_core
    gold_core
    mpsk_core
    burst_despreader_core
    loop_filter_core
    corr_core
    ${JM_MATH_LIBRARY})
target_include_directories(
  bench_dsss_burst_receiver_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
