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
add_library(ofdm_core OBJECT ofdm_core.c)
target_include_directories(
  ofdm_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                   ${CMAKE_SOURCE_DIR}/native/inc/doppler/ofdm)
target_link_libraries(ofdm_core PUBLIC
    fft_core)

add_executable(test_ofdm_core
               ${CMAKE_SOURCE_DIR}/native/tests/ofdm/test_ofdm_core.c
               ${CMAKE_SOURCE_DIR}/native/tests/ofdm/test_ofdm_symbols.c)
target_link_libraries(test_ofdm_core
                      PRIVATE ofdm_core fft_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_ofdm_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_ofdm_core COMMAND test_ofdm_core)

add_executable(
  bench_ofdm_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/ofdm/bench_ofdm_core.c)
target_link_libraries(bench_ofdm_core
                      PRIVATE ofdm_core fft_core
    ${JM_MATH_LIBRARY})
target_include_directories(
  bench_ofdm_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
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
# ofdm Python module — aggregates: Ofdm
Python3_add_library(ofdm MODULE WITH_SOABI ofdm_ext.c)
target_link_libraries(ofdm PRIVATE
    ofdm_core
    fft_core
    Python3::NumPy)
target_include_directories(ofdm PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(ofdm PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/ofdm"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/ofdm")
add_custom_command(TARGET ofdm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:ofdm>"
        "${PYTHON_PACKAGE_DIR}/ofdm/$<TARGET_FILE_NAME:ofdm>"
    VERBATIM
    COMMENT "Copy ofdm extension module")
endif()

# Your own CMake for this directory goes in ofdm_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/ofdm_extra.cmake OPTIONAL)
