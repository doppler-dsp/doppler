add_library(resample_core OBJECT resample_core.c ciccompmf.c kaiser_beta.c kaiser_num_taps.c)
target_include_directories(resample_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# resample Python module — aggregates: Resampler, HalfbandDecimator, CIC, RateConverter, Farrow, HalfbandDecimatorQ15, MatchedRateConverter
Python3_add_library(resample MODULE WITH_SOABI resample_ext.c)
target_link_libraries(resample PRIVATE
    resample_core
    Resampler_core
    HalfbandDecimator_core
    cic_core
    RateConverter_core
    farrow_core
    hbdecim_q15_core
    hbdecim_core
    hbdecim_r2c_core
    resamp_core
    fir_core
    agc_core
    dp_tlm_core
    m
    Python3::NumPy)
target_include_directories(resample PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(resample PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/resample"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/resample")
add_custom_command(TARGET resample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:resample>"
        "${PYTHON_PACKAGE_DIR}/resample/$<TARGET_FILE_NAME:resample>"
    VERBATIM
    COMMENT "Copy resample extension module")
endif()

add_executable(test_resample_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_resample_core.c)
target_link_libraries(test_resample_core PRIVATE
    resample_core
    Resampler_core
    HalfbandDecimator_core
    cic_core
    RateConverter_core
    farrow_core
    hbdecim_q15_core
    hbdecim_core
    hbdecim_r2c_core
    resamp_core
    fir_core
    agc_core
    dp_tlm_core
    m
    m)
target_include_directories(test_resample_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_resample_core COMMAND test_resample_core)
