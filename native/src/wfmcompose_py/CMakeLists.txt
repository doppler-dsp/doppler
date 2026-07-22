if(BUILD_PYTHON)

# wfmcompose_py — hand-written CPython binding for the one wfmgen transport
# primitive just-makeit cannot yet express: blue_write_hcb.
#
# no_generate module: just-makeit only wires this add_subdirectory; the .c, the
# compose.py wrapper, and the .pyi are hand-owned (like ddc_fn). The extension
# is named `_wfmcompose` and lands in doppler/wfm/, so it imports as
# doppler.wfm._wfmcompose — mirroring the hand-written doppler.wfm.readback.
#
# The capsule bindings for the now-generated handles were removed (gh-178 review
# #7), so the only backing left is wfm_blue_write_hcb (in wfm_writer_core, which
# uses cJSON for its embedded SigMF writer — hence wfm_cjson). The compose / dsp
# / sink / clock / synth / source cores it used to drag in are no longer linked,
# and the POSIX/Windows split is gone with the sink_*/clock_* bindings.

Python3_add_library(_wfmcompose MODULE WITH_SOABI wfmcompose_py.c)
target_include_directories(_wfmcompose PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/vendor/cjson)

target_link_libraries(_wfmcompose PRIVATE wfm_writer_core wfm_keywords_core wfm_draw_core wfm_cjson m)

set_target_properties(_wfmcompose PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/wfm"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/wfm")
add_custom_command(TARGET _wfmcompose POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:_wfmcompose>"
        "${PYTHON_PACKAGE_DIR}/wfm/$<TARGET_FILE_NAME:_wfmcompose>"
    VERBATIM
    COMMENT "Copy _wfmcompose extension module")

endif()
