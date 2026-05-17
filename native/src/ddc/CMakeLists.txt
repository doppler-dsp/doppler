add_library(ddc_core OBJECT ddc_core.c)
target_include_directories(ddc_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(ddc_core PUBLIC lo_core resamp_core hbdecim_r2c_core m)

# ddc Python module — Ddc, DdcR
Python3_add_library(ddc MODULE WITH_SOABI ddc_ext.c)
target_link_libraries(ddc PRIVATE
    ddc_core
    lo_core
    resamp_core
    hbdecim_r2c_core
    m
    Python3::NumPy)
target_include_directories(ddc PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(ddc PRIVATE -static-libgcc)
    get_filename_component(_gcc_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    foreach(_dll IN ITEMS libwinpthread-1.dll)
        if(EXISTS "${_gcc_bin}/${_dll}")
            add_custom_command(TARGET ddc POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_gcc_bin}/${_dll}"
                    "${PYTHON_PACKAGE_DIR}/ddc"
                VERBATIM)
        endif()
    endforeach()
endif()
set_target_properties(ddc PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/ddc"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/ddc")
add_custom_command(TARGET ddc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:ddc>"
        "${PYTHON_PACKAGE_DIR}/ddc/$<TARGET_FILE_NAME:ddc>"
    VERBATIM)
