if(BUILD_PYTHON)

# wfmcompose_py — hand-written CPython binding for the wfmgen composer subsystem
# (multi-segment composer, file writers, ZMQ sink, RRC/DSSS helpers).
#
# no_generate module: just-makeit only wires this add_subdirectory; the .c, the
# compose.py wrapper, and the .pyi are hand-owned (like ddc_fn). The extension
# is named `_wfmcompose` and lands in doppler/wfmgen/, so it imports as
# doppler.wfmgen._wfmcompose — mirroring the hand-written doppler.wfmgen.readback.
# The OBJECT libraries (wfm_*_core, wfm_cjson, stream_core_obj, zmq_vendor_static)
# come from native/src/wfmcompose (a c_dep added earlier in the Components block).

Python3_add_library(_wfmcompose MODULE WITH_SOABI wfmcompose_py.c)
target_include_directories(_wfmcompose PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/vendor/cjson)

if(WIN32)
    # No ZMQ sink on Windows (vendored zmq is POSIX-only); the binding #ifdefs
    # the sink_* functions out to match.
    target_link_libraries(_wfmcompose PRIVATE
        wfm_compose_core wfm_writer_core wfm_reader_core wfm_dsp_core wfm_cjson
        wfm_synth_core lo_core awgn_core pn_core
        m Python3::NumPy)
else()
    target_link_libraries(_wfmcompose PRIVATE
        wfm_compose_core wfm_writer_core wfm_reader_core wfm_sink_core
        timing_core wfm_dsp_core wfm_cjson
        wfm_synth_core lo_core awgn_core pn_core
        $<TARGET_OBJECTS:stream_core_obj>
        zmq_vendor_static
        Threads::Threads stdc++ m
        Python3::NumPy)
endif()

set_target_properties(_wfmcompose PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/wfmgen"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/wfmgen")
add_custom_command(TARGET _wfmcompose POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:_wfmcompose>"
        "${PYTHON_PACKAGE_DIR}/wfmgen/$<TARGET_FILE_NAME:_wfmcompose>"
    VERBATIM
    COMMENT "Copy _wfmcompose extension module")

endif()
