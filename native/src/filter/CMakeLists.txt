add_library(filter_core OBJECT filter_core.c)
target_include_directories(filter_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# filter Python module — aggregates: FIR, HBDecimQ15, MovingAverage
Python3_add_library(filter MODULE WITH_SOABI filter_ext.c)
target_link_libraries(filter PRIVATE
    filter_core
    fir_core
    hbdecim_q15_core
    boxcar_core
    Python3::NumPy)
target_include_directories(filter PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(filter PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/filter"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/filter")
add_custom_command(TARGET filter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:filter>"
        "${PYTHON_PACKAGE_DIR}/filter/$<TARGET_FILE_NAME:filter>"
    VERBATIM
    COMMENT "Copy filter extension module")
endif()
