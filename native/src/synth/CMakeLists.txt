# OBJECT library — pure C core, no Python dependency.
add_library(synth_core OBJECT synth_core.c)
target_include_directories(
  synth_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/synth)
target_link_libraries(synth_core PUBLIC
    lo_core
    awgn_core
    pn_core)

add_executable(test_synth_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_synth_core.c)
target_link_libraries(test_synth_core
                      PRIVATE synth_core lo_core
    awgn_core
    pn_core
    m)
target_include_directories(test_synth_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_synth_core COMMAND test_synth_core)

add_executable(
  bench_synth_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_synth_core.c)
target_link_libraries(bench_synth_core
                      PRIVATE synth_core lo_core
    awgn_core
    pn_core
    m)
target_include_directories(
  bench_synth_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
