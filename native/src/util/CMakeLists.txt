add_library(util_core OBJECT util_core.c)
target_include_directories(util_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# util Python module — aggregates: 
Python3_add_library(util MODULE WITH_SOABI util_ext.c)
target_link_libraries(util PRIVATE
    util_core
    Python3::NumPy)
target_include_directories(util PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(util PRIVATE -static-libgcc)
    get_filename_component(_gcc_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    foreach(_dll IN ITEMS libwinpthread-1.dll)
        if(EXISTS "${_gcc_bin}/${_dll}")
            add_custom_command(TARGET util POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_gcc_bin}/${_dll}"
                    "${PYTHON_PACKAGE_DIR}/util"
                VERBATIM)
        endif()
    endforeach()
endif()
set_target_properties(util PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/util"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/util")
add_custom_command(TARGET util POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:util>"
        "${PYTHON_PACKAGE_DIR}/util/$<TARGET_FILE_NAME:util>"
    VERBATIM)
endif()
