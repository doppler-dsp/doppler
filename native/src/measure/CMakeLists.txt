add_library(measure_core OBJECT measure_core.c measure_min_samples.c measure_rec_nfft.c measure_proc_gain.c dp_coherent_freq.c)
target_include_directories(measure_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# measure Python module — aggregates: ToneMeasure, NPRMeasure, IMDMeasure
Python3_add_library(measure MODULE WITH_SOABI measure_ext.c)
target_link_libraries(measure PRIVATE
    measure_core
    tonemeas_core
    nprmeas_core
    imdmeas_core
    fft_core
    spectral_core
    psd_core
    acc_trace_core
    Python3::NumPy)
target_include_directories(measure PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(measure PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/measure"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/measure")
add_custom_command(TARGET measure POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:measure>"
        "${PYTHON_PACKAGE_DIR}/measure/$<TARGET_FILE_NAME:measure>"
    VERBATIM
    COMMENT "Copy measure extension module")
endif()
