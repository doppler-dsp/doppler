add_library(coding_core OBJECT coding_core.c)
target_include_directories(coding_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# coding Python module — aggregates: ConvEncoder, Viterbi, ReedSolomon
Python3_add_library(coding MODULE WITH_SOABI coding_ext.c)
target_link_libraries(coding PRIVATE
    coding_core
    conv_enc_core
    viterbi_core
    rs_codec_core
    conv_core
    rs_core
    Python3::NumPy)
target_include_directories(coding PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(coding PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/coding"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/coding")
add_custom_command(TARGET coding POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:coding>"
        "${PYTHON_PACKAGE_DIR}/coding/$<TARGET_FILE_NAME:coding>"
    VERBATIM
    COMMENT "Copy coding extension module")
endif()
