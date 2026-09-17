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

add_library(accumulator_core OBJECT accumulator_core.c)
target_include_directories(accumulator_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# accumulator Python module — aggregates: AccF32, AccCf64, AccTrace
Python3_add_library(accumulator MODULE WITH_SOABI accumulator_ext.c)
target_link_libraries(accumulator PRIVATE
    accumulator_core
    acc_f32_core
    acc_cf64_core
    acc_trace_core
    Python3::NumPy)
target_include_directories(accumulator PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(accumulator PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/accumulator"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/accumulator")
add_custom_command(TARGET accumulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:accumulator>"
        "${PYTHON_PACKAGE_DIR}/accumulator/$<TARGET_FILE_NAME:accumulator>"
    VERBATIM
    COMMENT "Copy accumulator extension module")
endif()
