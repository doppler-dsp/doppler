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
add_library(nco_core OBJECT nco_core.c)
target_include_directories(
  nco_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                  ${CMAKE_SOURCE_DIR}/native/inc/doppler/nco)

add_executable(test_nco_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_nco_core.c
               ${CMAKE_SOURCE_DIR}/native/tests/test_nco_symbols.c)
target_link_libraries(test_nco_core
                      PRIVATE nco_core ${JM_MATH_LIBRARY})
target_include_directories(test_nco_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_nco_core COMMAND test_nco_core)

add_executable(
  bench_nco_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_nco_core.c)
target_link_libraries(bench_nco_core
                      PRIVATE nco_core ${JM_MATH_LIBRARY})
target_include_directories(
  bench_nco_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)

# Your own CMake for this directory goes in nco_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/nco_extra.cmake OPTIONAL)
