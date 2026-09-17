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

add_library(telemetry_core OBJECT telemetry_core.c)
target_include_directories(telemetry_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# telemetry Python module — aggregates: Telemetry, MemoryCapture, EventLog, Capture
Python3_add_library(telemetry MODULE WITH_SOABI telemetry_ext.c)
target_link_libraries(telemetry PRIVATE
    telemetry_core
    dp_tlm_core
    dp_tlm_capture_core
    dp_event_log_core
    Threads::Threads
    dp_interrupt_guard_core
    timing_core
    wfm_writer_core
    wfm_cjson_core
    wfm_keywords_core
    wfm_draw_core
    Python3::NumPy)
target_include_directories(telemetry PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(telemetry PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/telemetry"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/telemetry")
add_custom_command(TARGET telemetry POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:telemetry>"
        "${PYTHON_PACKAGE_DIR}/telemetry/$<TARGET_FILE_NAME:telemetry>"
    VERBATIM
    COMMENT "Copy telemetry extension module")
endif()
