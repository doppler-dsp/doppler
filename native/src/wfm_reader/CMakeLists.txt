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
add_library(wfm_reader_core OBJECT wfm_reader_core.c)
target_include_directories(
  wfm_reader_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                         ${CMAKE_SOURCE_DIR}/native/inc/wfm_reader)
target_link_libraries(wfm_reader_core PUBLIC
    dp_interrupt_guard_core
    wfm_cjson_core
    wfm_keywords_core
    wfm_writer_core
    wfm_draw_core)

add_executable(test_wfm_reader_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_wfm_reader_core.c
               ${CMAKE_SOURCE_DIR}/native/tests/test_wfm_reader_symbols.c)
target_link_libraries(test_wfm_reader_core
                      PRIVATE wfm_reader_core dp_interrupt_guard_core
    wfm_cjson_core
    wfm_keywords_core
    wfm_writer_core
    wfm_draw_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_wfm_reader_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_wfm_reader_core COMMAND test_wfm_reader_core)

add_executable(
  bench_wfm_reader_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_wfm_reader_core.c)
target_link_libraries(bench_wfm_reader_core
                      PRIVATE wfm_reader_core dp_interrupt_guard_core
    wfm_cjson_core
    wfm_keywords_core
    wfm_writer_core
    wfm_draw_core
    ${JM_MATH_LIBRARY})
target_include_directories(
  bench_wfm_reader_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
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
# wfm_reader Python module — aggregates: Reader
Python3_add_library(wfm_reader MODULE WITH_SOABI wfm_reader_ext.c)
target_link_libraries(wfm_reader PRIVATE
    wfm_reader_core
    dp_interrupt_guard_core
    wfm_cjson_core
    wfm_keywords_core
    wfm_writer_core
    wfm_draw_core
    Python3::NumPy)
target_include_directories(wfm_reader PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(wfm_reader PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/wfm"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/wfm")
add_custom_command(TARGET wfm_reader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:wfm_reader>"
        "${PYTHON_PACKAGE_DIR}/wfm/$<TARGET_FILE_NAME:wfm_reader>"
    VERBATIM
    COMMENT "Copy wfm_reader extension module")
endif()

# Your own CMake for this directory goes in wfm_reader_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/wfm_reader_extra.cmake OPTIONAL)
