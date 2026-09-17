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
add_library(capture_core OBJECT capture_core.c)
target_include_directories(
  capture_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                      ${CMAKE_SOURCE_DIR}/native/inc/capture)
target_link_libraries(capture_core PUBLIC
    doppler::doppler-static
    m)

add_executable(test_capture_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_capture_core.c)
target_link_libraries(test_capture_core
                      PRIVATE capture_core doppler::doppler-static
    m
    ${JM_MATH_LIBRARY})
target_include_directories(test_capture_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_capture_core COMMAND test_capture_core)

add_executable(
  bench_capture_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_capture_core.c)
target_link_libraries(bench_capture_core
                      PRIVATE capture_core doppler::doppler-static
    m
    ${JM_MATH_LIBRARY})
target_include_directories(
  bench_capture_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
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
# capture Python module — aggregates: Capture, RawCapture
Python3_add_library(capture MODULE WITH_SOABI capture_ext.c)
target_link_libraries(capture PRIVATE
    capture_core
    doppler::doppler-static
    m
    Python3::NumPy)
target_include_directories(capture PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(capture PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/capture"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/capture")
add_custom_command(TARGET capture POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:capture>"
        "${PYTHON_PACKAGE_DIR}/capture/$<TARGET_FILE_NAME:capture>"
    VERBATIM
    COMMENT "Copy capture extension module")
endif()
