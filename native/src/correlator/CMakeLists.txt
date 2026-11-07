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
add_library(correlator_core OBJECT correlator_core.c)
target_include_directories(
  correlator_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                         ${CMAKE_SOURCE_DIR}/native/inc/doppler/correlator)
target_link_libraries(correlator_core PUBLIC
    Threads::Threads
    fft2d_core
    fft_core
    detection_core
    dp_tlm_core
    cvt_core)
if(TARGET ${PROJECT_NAME}_lib)
  target_link_libraries(${PROJECT_NAME}_lib PRIVATE
      Threads::Threads)
  target_include_directories(${PROJECT_NAME}_lib INTERFACE
      $<$<TARGET_EXISTS:Threads::Threads>:$<TARGET_PROPERTY:Threads::Threads,INTERFACE_INCLUDE_DIRECTORIES>>)
  target_compile_definitions(${PROJECT_NAME}_lib INTERFACE
      $<$<TARGET_EXISTS:Threads::Threads>:$<TARGET_PROPERTY:Threads::Threads,INTERFACE_COMPILE_DEFINITIONS>>)
  target_compile_options(${PROJECT_NAME}_lib INTERFACE
      $<$<TARGET_EXISTS:Threads::Threads>:$<TARGET_PROPERTY:Threads::Threads,INTERFACE_COMPILE_OPTIONS>>)
endif()
if(TARGET ${PROJECT_NAME}_lib_static)
  target_link_libraries(${PROJECT_NAME}_lib_static PUBLIC
      Threads::Threads)
endif()

add_executable(test_correlator_core
               ${CMAKE_SOURCE_DIR}/native/tests/correlator/test_correlator_core.c
               ${CMAKE_SOURCE_DIR}/native/tests/correlator/test_correlator_symbols.c)
target_link_libraries(test_correlator_core
                      PRIVATE correlator_core Threads::Threads
    fft2d_core
    fft_core
    detection_core
    dp_tlm_core
    cvt_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_correlator_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_correlator_core COMMAND test_correlator_core)

add_executable(
  bench_correlator_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/correlator/bench_correlator_core.c)
target_link_libraries(bench_correlator_core
                      PRIVATE correlator_core Threads::Threads
    fft2d_core
    fft_core
    detection_core
    dp_tlm_core
    cvt_core
    ${JM_MATH_LIBRARY})
target_include_directories(
  bench_correlator_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)

# Your own CMake for this directory goes in correlator_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/correlator_extra.cmake OPTIONAL)
