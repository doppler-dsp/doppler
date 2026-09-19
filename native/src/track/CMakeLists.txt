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

add_library(track_core OBJECT track_core.c)
target_include_directories(track_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# track Python module — aggregates: LoopFilter, Costas, Dll, SymbolSync, RateSync, CarrierMpsk, CarrierNda, MpskReceiver, BpskReceiver, MpskReceiverR
Python3_add_library(track MODULE WITH_SOABI track_ext.c)
target_link_libraries(track PRIVATE
    track_core
    loop_filter_core
    costas_core
    dll_core
    symsync_core
    ratesync_core
    carrier_mpsk_core
    carrier_nda_core
    mpsk_receiver_core
    lo_core
    lockdet_core
    dp_tlm_core
    detection_core
    nco_core
    farrow_core
    RateConverter_core
    resamp_core
    fir_core
    agc_core
    hbdecim_core
    cic_core
    resample_core
    boxcar_core
    wfm_dsp_core
    ddc_core
    hbdecim_r2c_core
    ddcr_core
    Python3::NumPy)
target_include_directories(track PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(track PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/track"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/track")
add_custom_command(TARGET track POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:track>"
        "${PYTHON_PACKAGE_DIR}/track/$<TARGET_FILE_NAME:track>"
    VERBATIM
    COMMENT "Copy track extension module")
endif()

# Your own CMake for this directory goes in track_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/track_extra.cmake OPTIONAL)
