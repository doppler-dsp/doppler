# OBJECT library — pure C core, no Python dependency.
add_library(iq_file_core OBJECT iq_file_core.c)
target_include_directories(
  iq_file_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/iq_file)
target_link_libraries(iq_file_core PUBLIC
    wfm_sample_core)

add_executable(test_iq_file_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_iq_file_core.c)
target_link_libraries(test_iq_file_core
                      PRIVATE iq_file_core wfm_sample_core
    m)
target_include_directories(test_iq_file_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_iq_file_core COMMAND test_iq_file_core)

add_executable(
  bench_iq_file_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_iq_file_core.c)
target_link_libraries(bench_iq_file_core
                      PRIVATE iq_file_core wfm_sample_core
    m)
target_include_directories(
  bench_iq_file_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
