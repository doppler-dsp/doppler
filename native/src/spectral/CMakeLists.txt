add_library(spectral_core OBJECT spectral_core.c kaiser_enbw.c kaiser_window.c hann_window.c blackman_harris_window.c magnitude_db_cf32.c magnitude_db_cf64.c find_peaks_f32.c obw_from_power.c noise_floor_db.c)
target_include_directories(spectral_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# spectral Python module — aggregates: FFT, FFT2D, Corr, Corr2D, Detector, Detector2D, PSD
Python3_add_library(spectral MODULE WITH_SOABI spectral_ext.c)
target_link_libraries(spectral PRIVATE
    spectral_core
    fft_core
    fft2d_core
    corr_core
    corr2d_core
    detector_core
    detector2d_core
    psd_core
    acc_trace_core
    Python3::NumPy)
target_include_directories(spectral PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(spectral PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/spectral"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/spectral")
add_custom_command(TARGET spectral POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:spectral>"
        "${PYTHON_PACKAGE_DIR}/spectral/$<TARGET_FILE_NAME:spectral>"
    VERBATIM
    COMMENT "Copy spectral extension module")
endif()
