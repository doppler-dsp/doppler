add_library(spectral_core OBJECT spectral_core.c kaiser_enbw.c kaiser_window.c hann_window.c magnitude_db_cf32.c magnitude_db_cf64.c find_peaks_f32.c)
target_include_directories(spectral_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# spectral Python module — aggregates: FFT, FFT2D, Corr, Corr2D, Detector, Detector2D
Python3_add_library(spectral MODULE WITH_SOABI spectral_ext.c)
target_link_libraries(spectral PRIVATE
    spectral_core
    fft_core
    fft2d_core
    corr_core
    corr2d_core
    detector_core
    detector2d_core
    Python3::NumPy)
target_include_directories(spectral PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(spectral PRIVATE -static-libgcc)
    get_filename_component(gcc_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    foreach(dll_name IN ITEMS libwinpthread-1.dll)
        if(EXISTS "${gcc_bin}/${dll_name}")
            add_custom_command(TARGET spectral POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${gcc_bin}/${dll_name}"
                    "${PYTHON_PACKAGE_DIR}/spectral"
                VERBATIM
                COMMENT "Copy Windows runtime DLL ${dll_name}")
        endif()
    endforeach()
endif()
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
