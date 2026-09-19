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

add_library(source_core OBJECT source_core.c)
target_include_directories(source_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# source Python module — aggregates: NCO, LO, AWGN
Python3_add_library(source MODULE WITH_SOABI source_ext.c)
target_link_libraries(source PRIVATE
    source_core
    nco_core
    lo_core
    awgn_core
    $<$<AND:$<PLATFORM_ID:Linux>,$<STREQUAL:${CMAKE_SYSTEM_PROCESSOR},x86_64>>:mvec>
    Python3::NumPy)
target_include_directories(source PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(source PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/source"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/source")
add_custom_command(TARGET source POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:source>"
        "${PYTHON_PACKAGE_DIR}/source/$<TARGET_FILE_NAME:source>"
    VERBATIM
    COMMENT "Copy source extension module")
endif()

# Your own CMake for this directory goes in source_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/source_extra.cmake OPTIONAL)
