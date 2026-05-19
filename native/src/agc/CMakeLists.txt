# OBJECT library — pure C core, no Python dependency.
add_library(agc_core OBJECT agc_core.c)
target_include_directories(agc_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/agc)

add_executable(test_agc_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_agc_core.c)
target_link_libraries(test_agc_core PRIVATE agc_core m)
target_include_directories(test_agc_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_agc_core COMMAND test_agc_core)

add_executable(bench_agc_core
    ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_agc_core.c)
target_link_libraries(bench_agc_core PRIVATE agc_core m)
target_include_directories(bench_agc_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
            ${CMAKE_SOURCE_DIR}/native/benchmarks)
if(BUILD_PYTHON)
# agc Python module — aggregates: AGC
Python3_add_library(agc MODULE WITH_SOABI agc_ext.c)
target_link_libraries(agc PRIVATE
    agc_core
    Python3::NumPy)
target_include_directories(agc PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(agc PRIVATE -static-libgcc)
    get_filename_component(_gcc_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    foreach(_dll IN ITEMS libwinpthread-1.dll)
        if(EXISTS "${_gcc_bin}/${_dll}")
            add_custom_command(TARGET agc POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_gcc_bin}/${_dll}"
                    "${PYTHON_PACKAGE_DIR}/agc"
                VERBATIM)
        endif()
    endforeach()
endif()
set_target_properties(agc PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/agc"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/agc")
add_custom_command(TARGET agc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:agc>"
        "${PYTHON_PACKAGE_DIR}/agc/$<TARGET_FILE_NAME:agc>"
    VERBATIM)
endif()
