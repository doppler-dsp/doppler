# OBJECT library — pure C core, no Python dependency.
add_library(imdmeas_core OBJECT imdmeas_core.c)
target_include_directories(
  imdmeas_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/imdmeas)
target_link_libraries(imdmeas_core PUBLIC
    fft_core
    spectral_core
    psd_core
    acc_trace_core)

add_executable(test_imdmeas_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_imdmeas_core.c)
target_link_libraries(test_imdmeas_core
                      PRIVATE imdmeas_core fft_core
    spectral_core
    psd_core
    acc_trace_core
    m)
target_include_directories(test_imdmeas_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_imdmeas_core COMMAND test_imdmeas_core)

add_executable(
  bench_imdmeas_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_imdmeas_core.c)
target_link_libraries(bench_imdmeas_core
                      PRIVATE imdmeas_core fft_core
    spectral_core
    psd_core
    acc_trace_core
    m)
target_include_directories(
  bench_imdmeas_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
