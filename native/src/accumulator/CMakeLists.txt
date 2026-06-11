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
