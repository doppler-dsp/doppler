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
add_library(acq_core OBJECT acq_core.c)
target_include_directories(
  acq_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                  ${CMAKE_SOURCE_DIR}/native/inc/acq)
target_link_libraries(acq_core PUBLIC
    Threads::Threads
    corr2d_core
    fft2d_core
    fft_core
    detection_core
    dp_tlm_core)

add_executable(test_acq_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_acq_core.c)
target_link_libraries(test_acq_core
                      PRIVATE acq_core Threads::Threads
    corr2d_core
    fft2d_core
    fft_core
    detection_core
    dp_tlm_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_acq_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_acq_core COMMAND test_acq_core)

add_executable(
  bench_acq_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_acq_core.c)
target_link_libraries(bench_acq_core
                      PRIVATE acq_core Threads::Threads
    corr2d_core
    fft2d_core
    fft_core
    detection_core
    dp_tlm_core
    ${JM_MATH_LIBRARY})
target_include_directories(
  bench_acq_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
