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
add_library(interleaver_core OBJECT interleaver_core.c)
target_include_directories(
  interleaver_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                          ${CMAKE_SOURCE_DIR}/native/inc/doppler/interleaver)

add_executable(test_interleaver_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_interleaver_core.c
               ${CMAKE_SOURCE_DIR}/native/tests/test_interleaver_symbols.c)
target_link_libraries(test_interleaver_core
                      PRIVATE interleaver_core ${JM_MATH_LIBRARY})
target_include_directories(test_interleaver_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_interleaver_core COMMAND test_interleaver_core)

add_executable(
  bench_interleaver_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_interleaver_core.c)
target_link_libraries(bench_interleaver_core
                      PRIVATE interleaver_core ${JM_MATH_LIBRARY})
target_include_directories(
  bench_interleaver_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)

# Your own CMake for this directory goes in interleaver_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/interleaver_extra.cmake OPTIONAL)
