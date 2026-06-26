add_library(dsss_core OBJECT dsss_core.c)
target_include_directories(dsss_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# dsss Python module — aggregates: Despreader, Acquirer
Python3_add_library(dsss MODULE WITH_SOABI dsss_ext.c)
target_link_libraries(dsss PRIVATE
    dsss_core
    despreader_core
    acq_core
    loop_filter_core
    corr2d_core
    fft2d_core
    fft_core
    detection_core
    Python3::NumPy)
target_include_directories(dsss PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(dsss PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/dsss"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/dsss")
add_custom_command(TARGET dsss POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:dsss>"
        "${PYTHON_PACKAGE_DIR}/dsss/$<TARGET_FILE_NAME:dsss>"
    VERBATIM
    COMMENT "Copy dsss extension module")
endif()
