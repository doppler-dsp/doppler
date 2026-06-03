add_library(source_core OBJECT source_core.c)
target_include_directories(source_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# source Python module — aggregates: NCO, LO, AWGN
Python3_add_library(source MODULE WITH_SOABI source_ext.c)
target_link_libraries(source PRIVATE
    source_core
    nco_core
    lo_core
    awgn_core
    Python3::NumPy)
target_include_directories(source PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(source PRIVATE -static-libgcc)
    get_filename_component(gcc_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    foreach(dll_name IN ITEMS libwinpthread-1.dll)
        if(EXISTS "${gcc_bin}/${dll_name}")
            add_custom_command(TARGET source POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${gcc_bin}/${dll_name}"
                    "${PYTHON_PACKAGE_DIR}/source"
                VERBATIM
                COMMENT "Copy Windows runtime DLL ${dll_name}")
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
    VERBATIM
    COMMENT "Copy source extension module")
endif()
