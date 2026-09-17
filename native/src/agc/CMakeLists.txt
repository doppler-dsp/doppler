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
add_library(agc_core OBJECT agc_core.c settling_samples.c)
target_include_directories(
  agc_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                  ${CMAKE_SOURCE_DIR}/native/inc/agc)
target_link_libraries(agc_core PUBLIC
    dp_tlm_core)

add_executable(test_agc_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_agc_core.c)
target_link_libraries(test_agc_core
                      PRIVATE agc_core dp_tlm_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_agc_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_agc_core COMMAND test_agc_core)

add_executable(
  bench_agc_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_agc_core.c)
target_link_libraries(bench_agc_core
                      PRIVATE agc_core dp_tlm_core
    ${JM_MATH_LIBRARY})
target_include_directories(
  bench_agc_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
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
# agc Python module — aggregates: AGC
Python3_add_library(agc MODULE WITH_SOABI agc_ext.c)
target_link_libraries(agc PRIVATE
    agc_core
    dp_tlm_core
    Python3::NumPy)
target_include_directories(agc PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(agc PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/agc"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/agc")
add_custom_command(TARGET agc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:agc>"
        "${PYTHON_PACKAGE_DIR}/agc/$<TARGET_FILE_NAME:agc>"
    VERBATIM
    COMMENT "Copy agc extension module")
endif()
