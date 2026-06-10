add_library(detection_core OBJECT detection_core.c marcum_q.c det_threshold.c det_pd.c det_dwell.c det_snr.c det_threshold_power.c det_pd_power.c det_dwell_power.c det_snr_power.c)
target_include_directories(detection_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# detection Python module
Python3_add_library(detection MODULE WITH_SOABI detection_ext.c)
target_link_libraries(detection PRIVATE
    detection_core
    Python3::NumPy)
target_include_directories(detection PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(detection PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/detection"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/detection")
add_custom_command(TARGET detection POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:detection>"
        "${PYTHON_PACKAGE_DIR}/detection/$<TARGET_FILE_NAME:detection>"
    VERBATIM
    COMMENT "Copy detection extension module")
endif()
