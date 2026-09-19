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

add_library(impairment_core OBJECT impairment_core.c)
target_include_directories(impairment_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# impairment Python module — aggregates: DopplerChannel
Python3_add_library(impairment MODULE WITH_SOABI impairment_ext.c)
target_link_libraries(impairment PRIVATE
    impairment_core
    doppler_channel_core
    resamp_core
    Python3::NumPy)
target_include_directories(impairment PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(impairment PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/impairment"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/impairment")
add_custom_command(TARGET impairment POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:impairment>"
        "${PYTHON_PACKAGE_DIR}/impairment/$<TARGET_FILE_NAME:impairment>"
    VERBATIM
    COMMENT "Copy impairment extension module")
endif()

# Your own CMake for this directory goes in impairment_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/impairment_extra.cmake OPTIONAL)
