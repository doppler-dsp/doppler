# OBJECT library — pure C core, no Python dependency.
add_library(frame_core OBJECT frame_core.c)
target_include_directories(
  frame_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/frame)
target_link_libraries(frame_core PUBLIC
    wfm_frame_core
    ccsds_tm_core
    conv_core
    rs_core
    pn_core
    gold_core)

add_executable(test_frame_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_frame_core.c)
target_link_libraries(test_frame_core
                      PRIVATE frame_core wfm_frame_core
    ccsds_tm_core
    conv_core
    rs_core
    pn_core
    gold_core
    m)
target_include_directories(test_frame_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_frame_core COMMAND test_frame_core)

add_executable(
  bench_frame_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_frame_core.c)
target_link_libraries(bench_frame_core
                      PRIVATE frame_core wfm_frame_core
    ccsds_tm_core
    conv_core
    rs_core
    pn_core
    gold_core
    m)
target_include_directories(
  bench_frame_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
