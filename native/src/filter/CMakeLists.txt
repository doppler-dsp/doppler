add_library(filter_core OBJECT filter_core.c)
target_include_directories(filter_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# filter Python module — aggregates: Fir
Python3_add_library(filter MODULE WITH_SOABI filter_ext.c)
target_link_libraries(filter PRIVATE
    filter_core
    fir_core
    Python3::NumPy)
target_include_directories(filter PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(filter PRIVATE -static-libgcc)
    get_filename_component(_gcc_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    foreach(_dll IN ITEMS libwinpthread-1.dll)
        if(EXISTS "${_gcc_bin}/${_dll}")
            add_custom_command(TARGET filter POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_gcc_bin}/${_dll}"
                    "${PYTHON_PACKAGE_DIR}/filter"
                VERBATIM)
        endif()
    endforeach()
endif()
set_target_properties(filter PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/filter"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/filter")
add_custom_command(TARGET filter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:filter>"
        "${PYTHON_PACKAGE_DIR}/filter/$<TARGET_FILE_NAME:filter>"
    VERBATIM)
endif()
