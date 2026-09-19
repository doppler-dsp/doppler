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

add_library(spectral_core OBJECT spectral_core.c kaiser_enbw.c kaiser_window.c kaiser_beta_for_sidelobe.c hann_window.c blackman_harris_window.c magnitude_db_cf32.c magnitude_db_cf64.c find_peaks_f32.c obw_from_power.c noise_floor_db.c)
target_include_directories(spectral_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# spectral Python module — aggregates: FFT, FFT2D, Corr, Corr2D, CorrDetector, CorrDetector2D, PSD
Python3_add_library(spectral MODULE WITH_SOABI spectral_ext.c)
target_link_libraries(spectral PRIVATE
    spectral_core
    fft_core
    fft2d_core
    corr_core
    corr2d_core
    detector_core
    detector2d_core
    psd_core
    acc_trace_core
    Python3::NumPy)
target_include_directories(spectral PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(spectral PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/spectral"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/spectral")
add_custom_command(TARGET spectral POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:spectral>"
        "${PYTHON_PACKAGE_DIR}/spectral/$<TARGET_FILE_NAME:spectral>"
    VERBATIM
    COMMENT "Copy spectral extension module")
endif()

add_executable(test_spectral_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_spectral_core.c)
target_link_libraries(test_spectral_core PRIVATE
    spectral_core
    fft_core
    fft2d_core
    corr_core
    corr2d_core
    detector_core
    detector2d_core
    psd_core
    acc_trace_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_spectral_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_spectral_core COMMAND test_spectral_core)

# Your own CMake for this directory goes in spectral_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/spectral_extra.cmake OPTIONAL)
