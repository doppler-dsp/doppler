# OBJECT library — pure C core, no Python dependency.
add_library(dp_tlm_capture_core OBJECT dp_tlm_capture_core.c)
target_include_directories(
  dp_tlm_capture_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/dp_tlm_capture)
target_link_libraries(dp_tlm_capture_core PUBLIC
    Threads::Threads
    dp_tlm_core
    timing_core)

add_executable(test_dp_tlm_capture_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_dp_tlm_capture_core.c)
target_link_libraries(test_dp_tlm_capture_core
                      PRIVATE dp_tlm_capture_core Threads::Threads
    dp_tlm_core
    timing_core
    m)
target_include_directories(test_dp_tlm_capture_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_dp_tlm_capture_core COMMAND test_dp_tlm_capture_core)

add_executable(
  bench_dp_tlm_capture_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_dp_tlm_capture_core.c)
target_link_libraries(bench_dp_tlm_capture_core
                      PRIVATE dp_tlm_capture_core Threads::Threads
    dp_tlm_core
    timing_core
    m)
target_include_directories(
  bench_dp_tlm_capture_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
