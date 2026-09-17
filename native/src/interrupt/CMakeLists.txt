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

add_library(interrupt_core OBJECT interrupt_core.c)
target_include_directories(interrupt_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# interrupt Python module — aggregates: Interrupt
Python3_add_library(interrupt MODULE WITH_SOABI interrupt_ext.c)
target_link_libraries(interrupt PRIVATE
    interrupt_core
    dp_interrupt_guard_core
    $<TARGET_OBJECTS:dp_interrupt_obj>
    Python3::NumPy)
target_include_directories(interrupt PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(interrupt PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/interrupt"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/interrupt")
add_custom_command(TARGET interrupt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:interrupt>"
        "${PYTHON_PACKAGE_DIR}/interrupt/$<TARGET_FILE_NAME:interrupt>"
    VERBATIM
    COMMENT "Copy interrupt extension module")
endif()
