# OBJECT library — pure C core, no Python dependency.
add_library(ddc_core OBJECT ddc_core.c)
target_include_directories(
  ddc_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/ddc)
target_link_libraries(ddc_core PUBLIC
    m
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    hbdecim_r2c_core)

add_executable(test_ddc_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_ddc_core.c)
target_link_libraries(test_ddc_core
                      PRIVATE ddc_core m
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    hbdecim_r2c_core
    m)
target_include_directories(test_ddc_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_ddc_core COMMAND test_ddc_core)

add_executable(
  bench_ddc_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_ddc_core.c)
target_link_libraries(bench_ddc_core
                      PRIVATE ddc_core m
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    cic_core
    fir_core
    resample_core
    hbdecim_r2c_core
    m)
target_include_directories(
  bench_ddc_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
if(BUILD_PYTHON)
# ddc Python module — aggregates: DDC
Python3_add_library(ddc MODULE WITH_SOABI ddc_ext.c)
target_link_libraries(ddc PRIVATE
    ddc_core
    lo_core
    RateConverter_core
    resamp_core
    fir_core
    hbdecim_core
    hbdecim_r2c_core
    cic_core
    resample_core
    m
    Python3::NumPy)
target_include_directories(ddc PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(ddc PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/ddc"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/ddc")
add_custom_command(TARGET ddc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:ddc>"
        "${PYTHON_PACKAGE_DIR}/ddc/$<TARGET_FILE_NAME:ddc>"
    VERBATIM
    COMMENT "Copy ddc extension module")
endif()
