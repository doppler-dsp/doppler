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
add_library(syncword_core OBJECT syncword_core.c)
target_include_directories(
  syncword_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                       ${CMAKE_SOURCE_DIR}/native/inc/syncword)
target_link_libraries(syncword_core PUBLIC
    detection_core)

add_executable(test_syncword_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_syncword_core.c)
target_link_libraries(test_syncword_core
                      PRIVATE syncword_core detection_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_syncword_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_syncword_core COMMAND test_syncword_core)

add_executable(
  bench_syncword_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_syncword_core.c)
target_link_libraries(bench_syncword_core
                      PRIVATE syncword_core detection_core
    ${JM_MATH_LIBRARY})
target_include_directories(
  bench_syncword_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
