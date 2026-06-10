# C-level test — buffer/buffer.h is header-only (no _core.c to link). This is
# the only buffer gate on the macOS C job, which runs without BUILD_PYTHON, so
# it lives outside the guard below.
add_executable(test_buffer_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_buffer_core.c)
target_include_directories(test_buffer_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_buffer_core COMMAND test_buffer_core)

if(BUILD_PYTHON)
# buffer Python module — wraps buffer/buffer.h (header-only, no C source link)
Python3_add_library(buffer MODULE WITH_SOABI buffer_ext.c)
target_link_libraries(buffer PRIVATE Python3::NumPy)
target_include_directories(buffer PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc)
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(buffer PRIVATE -static-libgcc)
    get_filename_component(_gcc_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    foreach(_dll IN ITEMS libwinpthread-1.dll)
        if(EXISTS "${_gcc_bin}/${_dll}")
            add_custom_command(TARGET buffer POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_gcc_bin}/${_dll}"
                    "${PYTHON_PACKAGE_DIR}/buffer"
                VERBATIM)
        endif()
    endforeach()
endif()
set_target_properties(buffer PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/buffer"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/buffer")
add_custom_command(TARGET buffer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:buffer>"
        "${PYTHON_PACKAGE_DIR}/buffer/$<TARGET_FILE_NAME:buffer>"
    VERBATIM)
endif()
