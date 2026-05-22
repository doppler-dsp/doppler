add_library(detection_core OBJECT detection_core.c)
target_include_directories(detection_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# detection Python module — aggregates: 
Python3_add_library(detection MODULE WITH_SOABI detection_ext.c)
target_link_libraries(detection PRIVATE
    detection_core
    Python3::NumPy)
target_include_directories(detection PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(detection PRIVATE -static-libgcc)
    get_filename_component(_gcc_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    foreach(_dll IN ITEMS libwinpthread-1.dll)
        if(EXISTS "${_gcc_bin}/${_dll}")
            add_custom_command(TARGET detection POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_gcc_bin}/${_dll}"
                    "${PYTHON_PACKAGE_DIR}/detection"
                VERBATIM)
        endif()
    endforeach()
endif()
set_target_properties(detection PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/detection"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/detection")
add_custom_command(TARGET detection POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:detection>"
        "${PYTHON_PACKAGE_DIR}/detection/$<TARGET_FILE_NAME:detection>"
    VERBATIM)
endif()
