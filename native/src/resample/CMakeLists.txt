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

add_library(resample_core OBJECT resample_core.c ciccompmf.c kaiser_beta.c kaiser_num_taps.c)
target_include_directories(resample_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# resample Python module — aggregates: Resampler, HalfbandDecimator, CIC, RateConverter, Farrow, HalfbandDecimatorQ15, MatchedRateConverter
Python3_add_library(resample MODULE WITH_SOABI resample_ext.c)
target_link_libraries(resample PRIVATE
    resample_core
    Resampler_core
    HalfbandDecimator_core
    cic_core
    RateConverter_core
    farrow_core
    hbdecim_q15_core
    hbdecim_core
    hbdecim_r2c_core
    resamp_core
    fir_core
    agc_core
    dp_tlm_core
    Python3::NumPy)
target_include_directories(resample PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(resample PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/resample"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/resample")
add_custom_command(TARGET resample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:resample>"
        "${PYTHON_PACKAGE_DIR}/resample/$<TARGET_FILE_NAME:resample>"
    VERBATIM
    COMMENT "Copy resample extension module")
endif()

add_executable(test_resample_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_resample_core.c)
target_link_libraries(test_resample_core PRIVATE
    resample_core
    Resampler_core
    HalfbandDecimator_core
    cic_core
    RateConverter_core
    farrow_core
    hbdecim_q15_core
    hbdecim_core
    hbdecim_r2c_core
    resamp_core
    fir_core
    agc_core
    dp_tlm_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_resample_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_resample_core COMMAND test_resample_core)

# Your own CMake for this directory goes in resample_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/resample_extra.cmake OPTIONAL)
