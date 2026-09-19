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

add_library(filter_core OBJECT filter_core.c design_lowpass.c)
target_include_directories(filter_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# filter Python module — aggregates: FIR, MovingAverage
Python3_add_library(filter MODULE WITH_SOABI filter_ext.c)
target_link_libraries(filter PRIVATE
    filter_core
    fir_core
    boxcar_core
    spectral_core
    resample_core
    Python3::NumPy)
target_include_directories(filter PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(filter PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/filter"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/filter")
add_custom_command(TARGET filter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:filter>"
        "${PYTHON_PACKAGE_DIR}/filter/$<TARGET_FILE_NAME:filter>"
    VERBATIM
    COMMENT "Copy filter extension module")
endif()

add_executable(test_filter_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_filter_core.c)
target_link_libraries(test_filter_core PRIVATE
    filter_core
    fir_core
    boxcar_core
    spectral_core
    resample_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_filter_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_filter_core COMMAND test_filter_core)

# Your own CMake for this directory goes in filter_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/filter_extra.cmake OPTIONAL)
