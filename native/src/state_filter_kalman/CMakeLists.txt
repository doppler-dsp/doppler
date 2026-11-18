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
add_library(state_filter_kalman_core OBJECT state_filter_kalman_core.c)
target_include_directories(
  state_filter_kalman_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                                  ${CMAKE_SOURCE_DIR}/native/inc/doppler/state_filter_kalman)
target_link_libraries(state_filter_kalman_core PUBLIC
    state_filter_core)

add_executable(test_state_filter_kalman_core
               ${CMAKE_SOURCE_DIR}/native/tests/state_filter_kalman/test_state_filter_kalman_core.c
               ${CMAKE_SOURCE_DIR}/native/tests/state_filter_kalman/test_state_filter_kalman_symbols.c)
target_link_libraries(test_state_filter_kalman_core
                      PRIVATE state_filter_kalman_core state_filter_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_state_filter_kalman_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_state_filter_kalman_core COMMAND test_state_filter_kalman_core)

add_executable(
  bench_state_filter_kalman_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/state_filter_kalman/bench_state_filter_kalman_core.c)
target_link_libraries(bench_state_filter_kalman_core
                      PRIVATE state_filter_kalman_core state_filter_core
    ${JM_MATH_LIBRARY})
target_include_directories(
  bench_state_filter_kalman_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)

# Your own CMake for this directory goes in state_filter_kalman_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/state_filter_kalman_extra.cmake OPTIONAL)
