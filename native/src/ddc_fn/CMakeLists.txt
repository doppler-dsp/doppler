if(BUILD_PYTHON)

# ddc_fn — functional DDCR API (state passed as opaque capsule).
# This directory is NOT managed by just-makeit; edit it freely.
Python3_add_library(ddc_fn MODULE WITH_SOABI ddc_fn_ext.c)
target_link_libraries(ddc_fn PRIVATE
    ddc_core
    ddcr_core
    lo_core
    RateConverter_core
    resamp_core
    hbdecim_core
    hbdecim_r2c_core
    cic_core
    fir_core
    resample_core
    m
    Python3::NumPy)
target_include_directories(ddc_fn PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(ddc_fn PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/ddc"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/ddc")
add_custom_command(TARGET ddc_fn POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:ddc_fn>"
        "${PYTHON_PACKAGE_DIR}/ddc/$<TARGET_FILE_NAME:ddc_fn>"
    VERBATIM
    COMMENT "Copy ddc_fn extension module")

endif()
