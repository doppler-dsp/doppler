add_library(snr_core OBJECT snr_core.c snr_data_aided_db.c snr_m2m4_db.c snr_data_aided_db_series.c snr_m2m4_db_series.c)
target_include_directories(snr_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# snr Python module
Python3_add_library(snr MODULE WITH_SOABI snr_ext.c)
target_link_libraries(snr PRIVATE
    snr_core
    Python3::NumPy)
target_include_directories(snr PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(snr PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/snr"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/snr")
add_custom_command(TARGET snr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:snr>"
        "${PYTHON_PACKAGE_DIR}/snr/$<TARGET_FILE_NAME:snr>"
    VERBATIM
    COMMENT "Copy snr extension module")
endif()
