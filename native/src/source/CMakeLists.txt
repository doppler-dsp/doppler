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
    $<$<AND:$<PLATFORM_ID:Linux>,$<STREQUAL:${CMAKE_SYSTEM_PROCESSOR},x86_64>>:mvec>
    Python3::NumPy)
target_include_directories(source PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
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
