# wfmcompose — hand-owned c_dep: the multi-segment waveform composer (Phase B).
# Pure-C OBJECT library over the synth engine + a CTest. jm apply only emits the
# add_subdirectory for this dir (listed in [project].c_deps); this file is
# hand-maintained.
add_library(wfm_compose_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/wfmgen/wfm_compose.c)
target_include_directories(wfm_compose_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/wfmgen)
target_link_libraries(wfm_compose_core PUBLIC m)

add_executable(test_wfm_compose
    ${CMAKE_SOURCE_DIR}/native/tests/test_wfm_compose.c)
# the composer reuses synth, which pulls in lo/awgn/pn
target_link_libraries(test_wfm_compose PRIVATE
    wfm_compose_core synth_core lo_core awgn_core pn_core m)
target_include_directories(test_wfm_compose
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_wfm_compose COMMAND test_wfm_compose)
