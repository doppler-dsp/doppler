add_library(telemetry_core OBJECT telemetry_core.c)
target_include_directories(telemetry_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# telemetry Python module — aggregates: Telemetry, MemoryCapture, Capture
Python3_add_library(telemetry MODULE WITH_SOABI telemetry_ext.c)
target_link_libraries(telemetry PRIVATE
    telemetry_core
    dp_tlm_core
    dp_tlm_capture_core
    Threads::Threads
    dp_interrupt_guard_core
    timing_core
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
