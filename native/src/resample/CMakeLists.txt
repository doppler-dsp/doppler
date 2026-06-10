add_library(resample_core OBJECT resample_core.c ciccompmf.c kaiser_beta.c kaiser_num_taps.c)
target_include_directories(resample_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# resample Python module — aggregates: Resampler, HalfbandDecimator, CIC, RateConverter
Python3_add_library(resample MODULE WITH_SOABI resample_ext.c)
target_link_libraries(resample PRIVATE
    resample_core
    Resampler_core
    HalfbandDecimator_core
    cic_core
    RateConverter_core
    resamp_core
    hbdecim_core
    hbdecim_r2c_core
    resample_core
    fir_core
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
