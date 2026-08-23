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
