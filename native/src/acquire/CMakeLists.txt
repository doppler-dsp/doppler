add_library(acquire_core OBJECT acquire_core.c)
target_include_directories(acquire_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# acquire Python module — aggregates: CarrierAcquisition
Python3_add_library(acquire MODULE WITH_SOABI acquire_ext.c)
target_link_libraries(acquire PRIVATE
    acquire_core
    carrier_acq_core
    psd_core
    acc_trace_core
    detector_core
    detection_core
    spectral_core
    corr_core
    fft_core
    Python3::NumPy)
target_include_directories(acquire PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(acquire PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/acquire"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/acquire")
add_custom_command(TARGET acquire POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:acquire>"
        "${PYTHON_PACKAGE_DIR}/acquire/$<TARGET_FILE_NAME:acquire>"
    VERBATIM
    COMMENT "Copy acquire extension module")
endif()
