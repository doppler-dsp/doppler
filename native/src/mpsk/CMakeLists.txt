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

add_library(mpsk_core OBJECT mpsk_core.c mpsk_map.c mpsk_demap.c mpsk_diff_map.c mpsk_diff_demap.c mpsk_soft_demap.c mpsk_bits_per_symbol.c)
target_include_directories(mpsk_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# mpsk Python module
Python3_add_library(mpsk MODULE WITH_SOABI mpsk_ext.c)
target_link_libraries(mpsk PRIVATE
    mpsk_core
    Python3::NumPy)
target_include_directories(mpsk PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(mpsk PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/mpsk"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/mpsk")
add_custom_command(TARGET mpsk POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:mpsk>"
        "${PYTHON_PACKAGE_DIR}/mpsk/$<TARGET_FILE_NAME:mpsk>"
    VERBATIM
    COMMENT "Copy mpsk extension module")
endif()

# Your own CMake for this directory goes in mpsk_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/mpsk_extra.cmake OPTIONAL)
