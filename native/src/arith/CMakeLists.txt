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

add_library(arith_core OBJECT arith_core.c add_q15.c sub_q15.c mul_q15.c dot_q15.c shl_q15.c shr_q15.c add_q8.c sub_q8.c mul_q8.c dot_q8.c shl_q8.c shr_q8.c shl_i64.c shr_i64.c)
target_include_directories(arith_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# arith Python module — aggregates: AccQ15, AccQ8
Python3_add_library(arith MODULE WITH_SOABI arith_ext.c)
target_link_libraries(arith PRIVATE
    arith_core
    acc_q15_core
    acc_q8_core
    Python3::NumPy)
target_include_directories(arith PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(arith PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/arith"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/arith")
add_custom_command(TARGET arith POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:arith>"
        "${PYTHON_PACKAGE_DIR}/arith/$<TARGET_FILE_NAME:arith>"
    VERBATIM
    COMMENT "Copy arith extension module")
endif()

add_executable(test_arith_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_arith_core.c)
target_link_libraries(test_arith_core PRIVATE
    arith_core
    acc_q15_core
    acc_q8_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_arith_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_arith_core COMMAND test_arith_core)

# Your own CMake for this directory goes in arith_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/arith_extra.cmake OPTIONAL)
