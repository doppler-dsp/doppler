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
add_library(fft2d_core OBJECT fft2d_core.c)
target_include_directories(
  fft2d_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                    ${CMAKE_SOURCE_DIR}/native/inc/doppler/fft2d)
target_link_libraries(fft2d_core PUBLIC
    fft_core)

add_executable(test_fft2d_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_fft2d_core.c
               ${CMAKE_SOURCE_DIR}/native/tests/test_fft2d_symbols.c)
target_link_libraries(test_fft2d_core
                      PRIVATE fft2d_core fft_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_fft2d_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_fft2d_core COMMAND test_fft2d_core)

add_executable(
  bench_fft2d_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_fft2d_core.c)
target_link_libraries(bench_fft2d_core
                      PRIVATE fft2d_core fft_core
    ${JM_MATH_LIBRARY})
target_include_directories(
  bench_fft2d_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)

# Your own CMake for this directory goes in fft2d_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/fft2d_extra.cmake OPTIONAL)
