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
add_library(delay_core OBJECT delay_core.c)
target_include_directories(
  delay_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                    ${CMAKE_SOURCE_DIR}/native/inc/delay)

add_executable(test_delay_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_delay_core.c
               ${CMAKE_SOURCE_DIR}/native/tests/test_delay_symbols.c)
target_link_libraries(test_delay_core
                      PRIVATE delay_core ${JM_MATH_LIBRARY})
target_include_directories(test_delay_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_delay_core COMMAND test_delay_core)

add_executable(
  bench_delay_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_delay_core.c)
target_link_libraries(bench_delay_core
                      PRIVATE delay_core ${JM_MATH_LIBRARY})
target_include_directories(
  bench_delay_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
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
# delay Python module — aggregates: DelayCf64
Python3_add_library(delay MODULE WITH_SOABI delay_ext.c)
target_link_libraries(delay PRIVATE
    delay_core
    Python3::NumPy)
target_include_directories(delay PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(delay PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/delay"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/delay")
add_custom_command(TARGET delay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:delay>"
        "${PYTHON_PACKAGE_DIR}/delay/$<TARGET_FILE_NAME:delay>"
    VERBATIM
    COMMENT "Copy delay extension module")
endif()

# Your own CMake for this directory goes in delay_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/delay_extra.cmake OPTIONAL)
