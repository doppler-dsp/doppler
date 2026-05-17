add_library(accumulator_core OBJECT accumulator_core.c)
target_include_directories(accumulator_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)

# accumulator Python module — aggregates: AccF32, AccCf64
Python3_add_library(accumulator MODULE WITH_SOABI accumulator_ext.c)
target_link_libraries(accumulator PRIVATE
    accumulator_core
    acc_f32_core
    acc_cf64_core
    Python3::NumPy)
target_include_directories(accumulator PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(accumulator PRIVATE -static-libgcc)
    get_filename_component(_gcc_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    foreach(_dll IN ITEMS libwinpthread-1.dll)
        if(EXISTS "${_gcc_bin}/${_dll}")
            add_custom_command(TARGET accumulator POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_gcc_bin}/${_dll}"
                    "${PYTHON_PACKAGE_DIR}/accumulator"
                VERBATIM)
        endif()
    endforeach()
endif()
set_target_properties(accumulator PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/accumulator"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/accumulator")
add_custom_command(TARGET accumulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:accumulator>"
        "${PYTHON_PACKAGE_DIR}/accumulator/$<TARGET_FILE_NAME:accumulator>"
    VERBATIM)
