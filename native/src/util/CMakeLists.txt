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

add_library(util_core OBJECT util_core.c square_clip.c next_pow_two.c saturate.c ema_step.c ema_alpha_decim.c complement_power.c sinc.c mean_sinc.c simpson_weights.c midpoint_nodes.c gauss_hermite.c)
target_include_directories(util_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# util Python module
Python3_add_library(util MODULE WITH_SOABI util_ext.c)
target_link_libraries(util PRIVATE
    util_core
    Python3::NumPy)
target_include_directories(util PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(util PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/util"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/util")
add_custom_command(TARGET util POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:util>"
        "${PYTHON_PACKAGE_DIR}/util/$<TARGET_FILE_NAME:util>"
    VERBATIM
    COMMENT "Copy util extension module")
endif()

# Your own CMake for this directory goes in util_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/util_extra.cmake OPTIONAL)
