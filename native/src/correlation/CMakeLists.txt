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

add_library(correlation_core OBJECT correlation_core.c)
target_include_directories(correlation_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# correlation Python module — aggregates: Correlator
Python3_add_library(correlation MODULE WITH_SOABI correlation_ext.c)
target_link_libraries(correlation PRIVATE
    correlation_core
    correlator_core
    Threads::Threads
    fft2d_core
    fft_core
    detection_core
    dp_tlm_core
    Python3::NumPy)
target_include_directories(correlation PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(correlation PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/correlation"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/correlation")
add_custom_command(TARGET correlation POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:correlation>"
        "${PYTHON_PACKAGE_DIR}/correlation/$<TARGET_FILE_NAME:correlation>"
    VERBATIM
    COMMENT "Copy correlation extension module")
endif()

# Your own CMake for this directory goes in correlation_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/correlation_extra.cmake OPTIONAL)
