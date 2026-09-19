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

add_library(measure_core OBJECT measure_core.c measure_min_samples.c measure_rec_nfft.c measure_proc_gain.c dp_coherent_freq.c)
target_include_directories(measure_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# measure Python module — aggregates: ToneMeasure, NPRMeasure, IMDMeasure
Python3_add_library(measure MODULE WITH_SOABI measure_ext.c)
target_link_libraries(measure PRIVATE
    measure_core
    tonemeas_core
    nprmeas_core
    imdmeas_core
    fft_core
    spectral_core
    psd_core
    acc_trace_core
    Python3::NumPy)
target_include_directories(measure PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(measure PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/measure"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/measure")
add_custom_command(TARGET measure POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:measure>"
        "${PYTHON_PACKAGE_DIR}/measure/$<TARGET_FILE_NAME:measure>"
    VERBATIM
    COMMENT "Copy measure extension module")
endif()

add_executable(test_measure_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_measure_core.c)
target_link_libraries(test_measure_core PRIVATE
    measure_core
    tonemeas_core
    nprmeas_core
    imdmeas_core
    fft_core
    spectral_core
    psd_core
    acc_trace_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_measure_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_measure_core COMMAND test_measure_core)

# Your own CMake for this directory goes in measure_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/measure_extra.cmake OPTIONAL)
