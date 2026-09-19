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
add_library(mpsk_receiver_core OBJECT mpsk_receiver_core.c)
target_include_directories(
  mpsk_receiver_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/mpsk_receiver)
target_link_libraries(mpsk_receiver_core PUBLIC
    wfm_dsp_core
    ber_core
    ddc_core
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    agc_core
    dp_tlm_core
    hbdecim_r2c_core
    ddcr_core
    ratesync_core
    loop_filter_core
    lockdet_core
    symsync_core
    nco_core
    farrow_core
    detection_core
    pn_core)

add_executable(test_mpsk_receiver_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_mpsk_receiver_core.c
               ${CMAKE_SOURCE_DIR}/native/tests/test_mpsk_receiver_symbols.c)
target_link_libraries(test_mpsk_receiver_core
                      PRIVATE mpsk_receiver_core wfm_dsp_core
    ber_core
    ddc_core
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    agc_core
    dp_tlm_core
    hbdecim_r2c_core
    ddcr_core
    ratesync_core
    loop_filter_core
    lockdet_core
    symsync_core
    nco_core
    farrow_core
    detection_core
    pn_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_mpsk_receiver_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_mpsk_receiver_core COMMAND test_mpsk_receiver_core)

add_executable(
  bench_mpsk_receiver_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_mpsk_receiver_core.c)
target_link_libraries(bench_mpsk_receiver_core
                      PRIVATE mpsk_receiver_core wfm_dsp_core
    ber_core
    ddc_core
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    agc_core
    dp_tlm_core
    hbdecim_r2c_core
    ddcr_core
    ratesync_core
    loop_filter_core
    lockdet_core
    symsync_core
    nco_core
    farrow_core
    detection_core
    pn_core
    ${JM_MATH_LIBRARY})
target_include_directories(
  bench_mpsk_receiver_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)

# Your own CMake for this directory goes in mpsk_receiver_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/mpsk_receiver_extra.cmake OPTIONAL)
