add_library(resample_core OBJECT resample_core.c)
target_include_directories(resample_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# resample Python module — Resampler, HalfbandDecimator*
Python3_add_library(resample MODULE WITH_SOABI resample_ext.c)
target_link_libraries(resample PRIVATE
    resamp_core
    hbdecim_core
    hbdecim_r2c_core
    resample_core
    m
    Python3::NumPy)
target_include_directories(resample PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(resample PRIVATE -static-libgcc)
    get_filename_component(_gcc_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    foreach(_dll IN ITEMS libwinpthread-1.dll)
        if(EXISTS "${_gcc_bin}/${_dll}")
            add_custom_command(TARGET resample POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_gcc_bin}/${_dll}"
                    "${PYTHON_PACKAGE_DIR}/resample"
                VERBATIM)
        endif()
    endforeach()
endif()
set_target_properties(resample PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/resample"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/resample")
add_custom_command(TARGET resample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:resample>"
        "${PYTHON_PACKAGE_DIR}/resample/$<TARGET_FILE_NAME:resample>"
    VERBATIM)
endif()
