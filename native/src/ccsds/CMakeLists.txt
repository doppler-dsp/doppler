add_library(ccsds_core OBJECT ccsds_core.c asm_bits.c)
target_include_directories(ccsds_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# ccsds Python module
Python3_add_library(ccsds MODULE WITH_SOABI ccsds_ext.c)
target_link_libraries(ccsds PRIVATE
    ccsds_core
    ccsds_tm_core
    conv_core
    rs_core
    wfm_frame_core
    pn_core
    gold_core
    Python3::NumPy)
target_include_directories(ccsds PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(ccsds PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/ccsds"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/ccsds")
add_custom_command(TARGET ccsds POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:ccsds>"
        "${PYTHON_PACKAGE_DIR}/ccsds/$<TARGET_FILE_NAME:ccsds>"
    VERBATIM
    COMMENT "Copy ccsds extension module")
endif()

add_executable(test_ccsds_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_ccsds_core.c)
target_link_libraries(test_ccsds_core PRIVATE
    ccsds_core
    ccsds_tm_core
    conv_core
    rs_core
    wfm_frame_core
    pn_core
    gold_core
    m)
target_include_directories(test_ccsds_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_ccsds_core COMMAND test_ccsds_core)

add_executable(bench_ccsds_core
    ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_ccsds_core.c)
target_link_libraries(bench_ccsds_core PRIVATE
    ccsds_core
    ccsds_tm_core
    conv_core
    rs_core
    wfm_frame_core
    pn_core
    gold_core
    m)
target_include_directories(bench_ccsds_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
            ${CMAKE_SOURCE_DIR}/native/benchmarks)
