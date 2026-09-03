# OBJECT library — pure C core, no Python dependency.
add_library(dp_event_log_core OBJECT dp_event_log_core.c)
target_include_directories(
  dp_event_log_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/dp_event_log)
target_link_libraries(dp_event_log_core PUBLIC
    wfm_writer_core
    wfm_cjson_core
    wfm_keywords_core
    wfm_draw_core)

add_executable(test_dp_event_log_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_dp_event_log_core.c)
target_link_libraries(test_dp_event_log_core
                      PRIVATE dp_event_log_core wfm_writer_core
    wfm_cjson_core
    wfm_keywords_core
    wfm_draw_core
    m)
target_include_directories(test_dp_event_log_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_dp_event_log_core COMMAND test_dp_event_log_core)

add_executable(
  bench_dp_event_log_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_dp_event_log_core.c)
target_link_libraries(bench_dp_event_log_core
                      PRIVATE dp_event_log_core wfm_writer_core
    wfm_cjson_core
    wfm_keywords_core
    wfm_draw_core
    m)
target_include_directories(
  bench_dp_event_log_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
