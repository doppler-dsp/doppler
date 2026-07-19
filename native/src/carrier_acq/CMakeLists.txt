# OBJECT library — pure C core, no Python dependency.
add_library(carrier_acq_core OBJECT carrier_acq_core.c)
target_include_directories(
  carrier_acq_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/carrier_acq)
target_link_libraries(carrier_acq_core PUBLIC
    psd_core
    fft_core
    spectral_core
    acc_trace_core
    detector_core
    corr_core
    detection_core)

add_executable(test_carrier_acq_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_carrier_acq_core.c)
target_link_libraries(test_carrier_acq_core
                      PRIVATE carrier_acq_core psd_core
    fft_core
    spectral_core
    acc_trace_core
    detector_core
    corr_core
    detection_core
    m)
target_include_directories(test_carrier_acq_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_carrier_acq_core COMMAND test_carrier_acq_core)

add_executable(
  bench_carrier_acq_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_carrier_acq_core.c)
target_link_libraries(bench_carrier_acq_core
                      PRIVATE carrier_acq_core psd_core
    fft_core
    spectral_core
    acc_trace_core
    detector_core
    corr_core
    detection_core
    m)
target_include_directories(
  bench_carrier_acq_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
