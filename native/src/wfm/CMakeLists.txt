add_library(wfm_core OBJECT wfm_core.c bpsk_map.c qpsk_map.c wfm_awgn_amplitude.c wfm_ebno_to_snr_db.c mls_poly.c ccsds_asm_bits.c crc16.c rrc_h.c rc_h.c rrc_taps.c dsss_spread.c)
target_include_directories(wfm_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# wfm Python module — aggregates: PN, _SynthEngine, Gold, Frame, FrameDesc
Python3_add_library(wfm MODULE WITH_SOABI wfm_ext.c)
target_link_libraries(wfm PRIVATE
    wfm_core
    pn_core
    wfm_synth_core
    gold_core
    frame_core
    lo_core
    awgn_core
    fir_core
    resamp_core
    wfm_frame_core
    ccsds_tm_core
    conv_core
    rs_core
    wfm_dsp_core
    Python3::NumPy)
target_include_directories(wfm PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(wfm PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/wfm"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/wfm")
add_custom_command(TARGET wfm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:wfm>"
        "${PYTHON_PACKAGE_DIR}/wfm/$<TARGET_FILE_NAME:wfm>"
    VERBATIM
    COMMENT "Copy wfm extension module")
endif()

add_executable(test_wfm_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_wfm_core.c)
target_link_libraries(test_wfm_core PRIVATE
    wfm_core
    pn_core
    wfm_synth_core
    gold_core
    frame_core
    lo_core
    awgn_core
    fir_core
    resamp_core
    wfm_frame_core
    ccsds_tm_core
    conv_core
    rs_core
    wfm_dsp_core
    m)
target_include_directories(test_wfm_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_wfm_core COMMAND test_wfm_core)
