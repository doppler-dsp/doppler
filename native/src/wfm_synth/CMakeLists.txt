# OBJECT library — pure C core, no Python dependency.
add_library(wfm_synth_core OBJECT wfm_synth_core.c)
target_include_directories(
  wfm_synth_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/wfm_synth)
target_link_libraries(wfm_synth_core PUBLIC
    lo_core
    awgn_core
    pn_core
    fir_core
    wfm_dsp_core)

add_executable(test_wfm_synth_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_wfm_synth_core.c)
target_link_libraries(test_wfm_synth_core
                      PRIVATE wfm_synth_core lo_core
    awgn_core
    pn_core
    fir_core
    wfm_dsp_core
    m)
target_include_directories(test_wfm_synth_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_wfm_synth_core COMMAND test_wfm_synth_core)

add_executable(
  bench_wfm_synth_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_wfm_synth_core.c)
target_link_libraries(bench_wfm_synth_core
                      PRIVATE wfm_synth_core lo_core
    awgn_core
    pn_core
    fir_core
    wfm_dsp_core
    m)
target_include_directories(
  bench_wfm_synth_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
