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
add_library(symsync_core OBJECT symsync_core.c)
target_include_directories(
  symsync_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                      ${CMAKE_SOURCE_DIR}/native/inc/doppler/symsync)
target_link_libraries(symsync_core PUBLIC
    nco_core
    farrow_core
    loop_filter_core
    lockdet_core
    detection_core
    dp_tlm_core)

add_executable(test_symsync_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_symsync_core.c
               ${CMAKE_SOURCE_DIR}/native/tests/test_symsync_symbols.c)
target_link_libraries(test_symsync_core
                      PRIVATE symsync_core nco_core
    farrow_core
    loop_filter_core
    lockdet_core
    detection_core
    dp_tlm_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_symsync_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_symsync_core COMMAND test_symsync_core)

add_executable(
  bench_symsync_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_symsync_core.c)
target_link_libraries(bench_symsync_core
                      PRIVATE symsync_core nco_core
    farrow_core
    loop_filter_core
    lockdet_core
    detection_core
    dp_tlm_core
    ${JM_MATH_LIBRARY})
target_include_directories(
  bench_symsync_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)

# Your own CMake for this directory goes in symsync_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/symsync_extra.cmake OPTIONAL)
