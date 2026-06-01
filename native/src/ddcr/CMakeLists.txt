# OBJECT library — pure C core, no Python dependency.
add_library(ddcr_core OBJECT ddcr_core.c)
target_include_directories(
  ddcr_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/ddcr)

add_executable(test_ddcr_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_ddcr_core.c)
target_link_libraries(test_ddcr_core PRIVATE ddcr_core ddc_core
    lo_core RateConverter_core resamp_core hbdecim_core hbdecim_r2c_core
    cic_core fir_core resample_core m)
target_include_directories(test_ddcr_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_ddcr_core COMMAND test_ddcr_core)

add_executable(
  bench_ddcr_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_ddcr_core.c)
target_link_libraries(bench_ddcr_core PRIVATE ddcr_core ddc_core
    lo_core RateConverter_core resamp_core hbdecim_core hbdecim_r2c_core
    cic_core fir_core resample_core m)
target_include_directories(
  bench_ddcr_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
