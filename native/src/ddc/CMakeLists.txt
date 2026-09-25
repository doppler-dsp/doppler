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
add_library(ddc_core OBJECT ddc_core.c)
target_include_directories(
  ddc_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                  ${CMAKE_SOURCE_DIR}/native/inc/doppler/ddc)
target_link_libraries(ddc_core PUBLIC
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    agc_core
    dp_tlm_core
    hbdecim_r2c_core)

add_executable(test_ddc_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_ddc_core.c
               ${CMAKE_SOURCE_DIR}/native/tests/test_ddc_symbols.c)
target_link_libraries(test_ddc_core
                      PRIVATE ddc_core lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    agc_core
    dp_tlm_core
    hbdecim_r2c_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_ddc_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_ddc_core COMMAND test_ddc_core)

add_executable(
  bench_ddc_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_ddc_core.c)
target_link_libraries(bench_ddc_core
                      PRIVATE ddc_core lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    agc_core
    dp_tlm_core
    hbdecim_r2c_core
    ${JM_MATH_LIBRARY})
target_include_directories(
  bench_ddc_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
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

if(BUILD_PYTHON)
# ddc Python module — aggregates: DDC, Ddcr, MatchedDDC, MatchedDdcr
Python3_add_library(ddc MODULE WITH_SOABI ddc_ext.c)
target_link_libraries(ddc PRIVATE
    ddc_core
    ddcr_core
    lo_core
    RateConverter_core
    resamp_core
    fir_core
    agc_core
    dp_tlm_core
    hbdecim_core
    hbdecim_r2c_core
    cic_core
    resample_core
    Python3::NumPy)
target_include_directories(ddc PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(ddc PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/ddc"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/ddc")
add_custom_command(TARGET ddc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:ddc>"
        "${PYTHON_PACKAGE_DIR}/ddc/$<TARGET_FILE_NAME:ddc>"
    VERBATIM
    COMMENT "Copy ddc extension module")
endif()

# Your own CMake for this directory goes in ddc_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/ddc_extra.cmake OPTIONAL)
