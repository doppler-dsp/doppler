# OBJECT library — pure C core, no Python dependency.
add_library(wfm_reader_core OBJECT wfm_reader_core.c)
target_include_directories(
  wfm_reader_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/wfm_reader)
target_link_libraries(wfm_reader_core PUBLIC
    m
    wfm_cjson_core
    wfm_keywords_core
    wfm_writer_core
    wfm_draw_core)

add_executable(test_wfm_reader_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_wfm_reader_core.c)
target_link_libraries(test_wfm_reader_core
                      PRIVATE wfm_reader_core m
    wfm_cjson_core
    wfm_keywords_core
    wfm_writer_core
    wfm_draw_core
    m)
target_include_directories(test_wfm_reader_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_wfm_reader_core COMMAND test_wfm_reader_core)

add_executable(
  bench_wfm_reader_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_wfm_reader_core.c)
target_link_libraries(bench_wfm_reader_core
                      PRIVATE wfm_reader_core m
    wfm_cjson_core
    wfm_keywords_core
    wfm_writer_core
    wfm_draw_core
    m)
target_include_directories(
  bench_wfm_reader_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
if(BUILD_PYTHON)
# wfm_reader Python module — aggregates: Reader
Python3_add_library(wfm_reader MODULE WITH_SOABI wfm_reader_ext.c)
target_link_libraries(wfm_reader PRIVATE
    wfm_reader_core
    wfm_cjson_core
    wfm_keywords_core
    wfm_writer_core
    wfm_draw_core
    m
    Python3::NumPy)
target_include_directories(wfm_reader PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(wfm_reader PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/wfm"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/wfm")
add_custom_command(TARGET wfm_reader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:wfm_reader>"
        "${PYTHON_PACKAGE_DIR}/wfm/$<TARGET_FILE_NAME:wfm_reader>"
    VERBATIM
    COMMENT "Copy wfm_reader extension module")
endif()
