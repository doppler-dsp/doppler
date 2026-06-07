# wfmcompose — hand-owned c_dep: the multi-segment waveform composer + output
# containers (Phase B/C). Pure-C OBJECT libraries over the synth engine; jm
# apply only emits the add_subdirectory for this dir (listed in
# [project].c_deps). This file is hand-maintained.

# ── vendored cJSON (compiled once, shared by the JSON + writer libs) ──────────
add_library(wfm_cjson OBJECT ${CMAKE_SOURCE_DIR}/vendor/cjson/cJSON.c)
target_include_directories(wfm_cjson PUBLIC ${CMAKE_SOURCE_DIR}/vendor/cjson)

# ── composer + JSON spec ─────────────────────────────────────────────────────
add_library(wfm_compose_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/wfmgen/wfm_compose.c
    ${CMAKE_SOURCE_DIR}/native/src/wfmgen/wfm_json.c)
target_include_directories(wfm_compose_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/wfmgen
    ${CMAKE_SOURCE_DIR}/vendor/cjson)
target_link_libraries(wfm_compose_core PUBLIC m)

add_executable(test_wfm_compose
    ${CMAKE_SOURCE_DIR}/native/tests/test_wfm_compose.c)
# the composer reuses synth, which pulls in lo/awgn/pn
target_link_libraries(test_wfm_compose PRIVATE
    wfm_compose_core wfm_cjson synth_core lo_core awgn_core pn_core m)
target_include_directories(test_wfm_compose PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc ${CMAKE_SOURCE_DIR}/vendor/cjson)
add_test(NAME test_wfm_compose COMMAND test_wfm_compose)

# ── DSSS spreading + RRC pulse-shaping taps (pure DSP, no deps) ──────────────
add_library(wfm_dsp_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/wfmgen/wfm_dsp.c)
target_include_directories(wfm_dsp_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/wfmgen)
target_link_libraries(wfm_dsp_core PUBLIC m)

add_executable(test_wfm_dsp
    ${CMAKE_SOURCE_DIR}/native/tests/test_wfm_dsp.c)
target_link_libraries(test_wfm_dsp PRIVATE wfm_dsp_core m)
target_include_directories(test_wfm_dsp
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_wfm_dsp COMMAND test_wfm_dsp)

# ── output containers: raw/csv/BLUE-1000 + SigMF meta (cJSON) ────────────────
add_library(wfm_writer_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/wfmgen/wfm_writer.c)
target_include_directories(wfm_writer_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/wfmgen
    ${CMAKE_SOURCE_DIR}/vendor/cjson)
target_link_libraries(wfm_writer_core PUBLIC m)

add_executable(test_wfm_writer
    ${CMAKE_SOURCE_DIR}/native/tests/test_wfm_writer.c)
target_link_libraries(test_wfm_writer PRIVATE wfm_writer_core wfm_cjson m)
target_include_directories(test_wfm_writer PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc ${CMAKE_SOURCE_DIR}/vendor/cjson)
add_test(NAME test_wfm_writer COMMAND test_wfm_writer)

# ── ZMQ PUB sink — POSIX only (vendored zmq + pthreads don't build on MinGW;
#    mirrors examples/c's streaming guard). The sink is independent of the
#    composer, so it links the stream layer, not synth. ──────────────────────
if(NOT WIN32)
    find_package(Threads REQUIRED)
    add_library(wfm_sink_core OBJECT
        ${CMAKE_SOURCE_DIR}/native/src/wfmgen/wfm_sink.c)
    target_include_directories(wfm_sink_core PUBLIC
        ${CMAKE_SOURCE_DIR}/native/inc
        ${CMAKE_SOURCE_DIR}/native/inc/wfmgen)
    target_link_libraries(wfm_sink_core PUBLIC m)

    add_executable(test_wfm_sink
        ${CMAKE_SOURCE_DIR}/native/tests/test_wfm_sink.c)
    target_link_libraries(test_wfm_sink PRIVATE
        wfm_sink_core
        $<TARGET_OBJECTS:stream_core_obj>
        zmq_vendor_static
        Threads::Threads stdc++ m)
    target_include_directories(test_wfm_sink PRIVATE
        ${CMAKE_SOURCE_DIR}/native/inc)
    add_test(NAME test_wfm_sink COMMAND test_wfm_sink)
endif()
