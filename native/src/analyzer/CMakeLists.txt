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

add_library(analyzer_core OBJECT analyzer_core.c)
target_include_directories(analyzer_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# analyzer Python module — aggregates: Specan
Python3_add_library(analyzer MODULE WITH_SOABI analyzer_ext.c)
target_link_libraries(analyzer PRIVATE
    analyzer_core
    specan_core
    ddc_core
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
    psd_core
    acc_trace_core
    fft_core
    spectral_core
    m
    Python3::NumPy)
target_include_directories(analyzer PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(analyzer PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/analyzer"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/analyzer")
add_custom_command(TARGET analyzer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:analyzer>"
        "${PYTHON_PACKAGE_DIR}/analyzer/$<TARGET_FILE_NAME:analyzer>"
    VERBATIM
    COMMENT "Copy analyzer extension module")
endif()
