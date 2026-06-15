add_library(analyzer_core OBJECT analyzer_core.c)
target_include_directories(analyzer_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# analyzer Python module — aggregates: Specan
Python3_add_library(analyzer MODULE WITH_SOABI analyzer_ext.c)
target_link_libraries(analyzer PRIVATE
    analyzer_core
    specan_core
    ddc_core
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    hbdecim_r2c_core
    cic_core
    fir_core
    resample_core
    welch_core
    acc_trace_core
    fft_core
    spectral_core
    m
    Python3::NumPy)
target_include_directories(analyzer PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(analyzer PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/analyzer"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/analyzer")
add_custom_command(TARGET analyzer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:analyzer>"
        "${PYTHON_PACKAGE_DIR}/analyzer/$<TARGET_FILE_NAME:analyzer>"
    VERBATIM
    COMMENT "Copy analyzer extension module")
endif()
