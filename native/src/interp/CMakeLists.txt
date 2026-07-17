add_library(interp_core OBJECT interp_core.c)
target_include_directories(interp_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# interp Python module — aggregates: InterpolatedTable
Python3_add_library(interp MODULE WITH_SOABI interp_ext.c)
target_link_libraries(interp PRIVATE
    interp_core
    interp_table_core
    Python3::NumPy)
target_include_directories(interp PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(interp PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/interp"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/interp")
add_custom_command(TARGET interp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:interp>"
        "${PYTHON_PACKAGE_DIR}/interp/$<TARGET_FILE_NAME:interp>"
    VERBATIM
    COMMENT "Copy interp extension module")
endif()
