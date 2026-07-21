add_library(impairment_core OBJECT impairment_core.c)
target_include_directories(impairment_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# impairment Python module — aggregates: DopplerChannel
Python3_add_library(impairment MODULE WITH_SOABI impairment_ext.c)
target_link_libraries(impairment PRIVATE
    impairment_core
    doppler_channel_core
    resamp_core
    Python3::NumPy)
target_include_directories(impairment PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(impairment PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/impairment"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/impairment")
add_custom_command(TARGET impairment POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:impairment>"
        "${PYTHON_PACKAGE_DIR}/impairment/$<TARGET_FILE_NAME:impairment>"
    VERBATIM
    COMMENT "Copy impairment extension module")
endif()
