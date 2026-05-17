add_library(source_core OBJECT source_core.c)
target_include_directories(source_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)

# source Python module — aggregates: NCO, LO
Python3_add_library(source MODULE WITH_SOABI source_ext.c)
target_link_libraries(source PRIVATE
    source_core
    nco_core
    lo_core
    Python3::NumPy
    m)
target_include_directories(source PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(source PRIVATE -static-libgcc)
    get_filename_component(_gcc_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    foreach(_dll IN ITEMS libwinpthread-1.dll)
        if(EXISTS "${_gcc_bin}/${_dll}")
            add_custom_command(TARGET source POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_gcc_bin}/${_dll}"
                    "${PYTHON_PACKAGE_DIR}/source"
                VERBATIM)
        endif()
    endforeach()
endif()
set_target_properties(source PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/source"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/source")
add_custom_command(TARGET source POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:source>"
        "${PYTHON_PACKAGE_DIR}/source/$<TARGET_FILE_NAME:source>"
    VERBATIM)
