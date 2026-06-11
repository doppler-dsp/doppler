# wfmcompose — hand-owned c_dep: the multi-segment waveform composer + output
# containers (Phase B/C). Pure-C OBJECT libraries over the synth engine; jm
# apply only emits the add_subdirectory for this dir (listed in
# [project].c_deps). This file is hand-maintained.

# ── vendored cJSON (compiled once, shared by the JSON + writer libs) ──────────
add_library(wfm_cjson OBJECT ${CMAKE_SOURCE_DIR}/vendor/cjson/cJSON.c)
target_include_directories(wfm_cjson PUBLIC ${CMAKE_SOURCE_DIR}/vendor/cjson)

# ── composer + JSON spec ─────────────────────────────────────────────────────
add_library(wfm_compose_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/wfm/wfm_compose.c
    ${CMAKE_SOURCE_DIR}/native/src/wfm/wfm_json.c
    ${CMAKE_SOURCE_DIR}/native/src/wfm/wfm_resolve.c)
target_include_directories(wfm_compose_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/wfm
    ${CMAKE_SOURCE_DIR}/vendor/cjson)
target_link_libraries(wfm_compose_core PUBLIC m)

add_executable(test_wfm_compose
    ${CMAKE_SOURCE_DIR}/native/tests/test_wfm_compose.c)
# the composer reuses synth, which pulls in lo/awgn/pn
target_link_libraries(test_wfm_compose PRIVATE
    wfm_compose_core wfm_cjson wfm_synth_core lo_core awgn_core pn_core m)
target_include_directories(test_wfm_compose PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc ${CMAKE_SOURCE_DIR}/vendor/cjson)
add_test(NAME test_wfm_compose COMMAND test_wfm_compose)

# ── DSSS spreading + RRC pulse-shaping taps (pure DSP, no deps) ──────────────
add_library(wfm_dsp_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/wfm/wfm_dsp.c)
target_include_directories(wfm_dsp_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/wfm)
target_link_libraries(wfm_dsp_core PUBLIC m)

add_executable(test_wfm_dsp
    ${CMAKE_SOURCE_DIR}/native/tests/test_wfm_dsp.c)
target_link_libraries(test_wfm_dsp PRIVATE wfm_dsp_core m)
target_include_directories(test_wfm_dsp
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_wfm_dsp COMMAND test_wfm_dsp)

# ── output containers: raw/csv/BLUE-1000 + SigMF meta (cJSON) ────────────────
add_library(wfm_writer_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/wfm/wfm_writer.c)
target_include_directories(wfm_writer_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/wfm
    ${CMAKE_SOURCE_DIR}/vendor/cjson)
target_link_libraries(wfm_writer_core PUBLIC m)

add_executable(test_wfm_writer
    ${CMAKE_SOURCE_DIR}/native/tests/test_wfm_writer.c)
target_link_libraries(test_wfm_writer PRIVATE wfm_writer_core wfm_cjson m)
target_include_directories(test_wfm_writer PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc ${CMAKE_SOURCE_DIR}/vendor/cjson)
add_test(NAME test_wfm_writer COMMAND test_wfm_writer)

# ── input containers: the dual of the writer (raw/csv/BLUE/SigMF auto-detect) ─
add_library(wfm_reader_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/wfm/wfm_reader.c)
target_include_directories(wfm_reader_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/wfm
    ${CMAKE_SOURCE_DIR}/vendor/cjson)
target_link_libraries(wfm_reader_core PUBLIC m)

add_executable(test_wfm_reader
    ${CMAKE_SOURCE_DIR}/native/tests/test_wfm_reader.c)
target_link_libraries(test_wfm_reader PRIVATE
    wfm_reader_core wfm_writer_core wfm_cjson m)
target_include_directories(test_wfm_reader PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc ${CMAKE_SOURCE_DIR}/vendor/cjson)
add_test(NAME test_wfm_reader COMMAND test_wfm_reader)

# ── ZMQ PUB sink — POSIX only (vendored zmq + pthreads don't build on MinGW;
#    mirrors examples/c's streaming guard). The sink is independent of the
#    composer, so it links the stream layer, not synth. ──────────────────────
if(NOT WIN32)
    find_package(Threads REQUIRED)

    # ── sample-clock pacing + timestamping (timing_core) — POSIX (clock_*),
    #    so it sits under the same guard as the sink. Generic (no wfmgen deps);
    #    consumed by both wfmgen_cli (--realtime) and _wfmcompose. ────────────
    add_library(timing_core OBJECT
        ${CMAKE_SOURCE_DIR}/native/src/timing/timing_core.c)
    target_include_directories(timing_core PUBLIC
        ${CMAKE_SOURCE_DIR}/native/inc)

    # Expose the sample clock in libdoppler (.so + .a) for downstream C — the
    # header already ships via the native/inc install. doppler_lib /
    # doppler_lib_static are defined in the root CMakeLists before this
    # add_subdirectory, so referencing their object sets here is safe and keeps
    # the change out of the jm-managed Components block. (The other wfmcompose
    # cores stay app/binding-only by design; timing_core is the one general-
    # purpose primitive worth shipping.) POSIX-only, like timing_core itself.
    target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:timing_core>)
    target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:timing_core>)

    add_executable(test_timing_core
        ${CMAKE_SOURCE_DIR}/native/tests/test_timing_core.c)
    target_link_libraries(test_timing_core PRIVATE timing_core)
    target_include_directories(test_timing_core PRIVATE
        ${CMAKE_SOURCE_DIR}/native/inc)
    add_test(NAME test_timing_core COMMAND test_timing_core)

    add_executable(bench_timing_core
        ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_timing_core.c)
    target_link_libraries(bench_timing_core PRIVATE timing_core m)
    target_include_directories(bench_timing_core PRIVATE
        ${CMAKE_SOURCE_DIR}/native/inc ${CMAKE_SOURCE_DIR}/native/benchmarks)

    # Container codec benchmarks (make bench). POSIX-only too: the writer bench
    # isolates the codec via open_memstream and the reader bench uses temp
    # files — neither is on the Windows portability path (no Windows wheels).
    add_executable(bench_wfm_writer_core
        ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_wfm_writer_core.c)
    target_link_libraries(bench_wfm_writer_core PRIVATE
        wfm_writer_core wfm_cjson m)
    target_include_directories(bench_wfm_writer_core PRIVATE
        ${CMAKE_SOURCE_DIR}/native/inc ${CMAKE_SOURCE_DIR}/native/benchmarks
        ${CMAKE_SOURCE_DIR}/vendor/cjson)

    add_executable(bench_wfm_reader_core
        ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_wfm_reader_core.c)
    target_link_libraries(bench_wfm_reader_core PRIVATE
        wfm_reader_core wfm_writer_core wfm_cjson m)
    target_include_directories(bench_wfm_reader_core PRIVATE
        ${CMAKE_SOURCE_DIR}/native/inc ${CMAKE_SOURCE_DIR}/native/benchmarks
        ${CMAKE_SOURCE_DIR}/vendor/cjson)

    add_library(wfm_sink_core OBJECT
        ${CMAKE_SOURCE_DIR}/native/src/wfm/wfm_sink.c)
    target_include_directories(wfm_sink_core PUBLIC
        ${CMAKE_SOURCE_DIR}/native/inc
        ${CMAKE_SOURCE_DIR}/native/inc/wfm)
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

    # ── wfmgen: the composer CLI (multi-segment / BLUE / SigMF / zmq / record).
    #    Hand-written (a composer is not a single-object jm-app generator); thin
    #    glue over wfm_compose + wfm_writer + wfm_sink. POSIX-only (zmq). Target
    #    is wfmgen_cli (the module target already owns the name `wfmgen`); the
    #    binary output is `wfmgen`. ───────────────────────────────────────────
    add_executable(wfmgen_cli ${CMAKE_SOURCE_DIR}/native/src/app/wfmgen.c)
    set_target_properties(wfmgen_cli PROPERTIES OUTPUT_NAME wfmgen)
    target_link_libraries(wfmgen_cli PRIVATE
        wfm_compose_core wfm_writer_core wfm_sink_core timing_core wfm_cjson
        wfm_synth_core lo_core awgn_core pn_core
        $<TARGET_OBJECTS:stream_core_obj>
        zmq_vendor_static
        Threads::Threads stdc++ m)
    target_include_directories(wfmgen_cli PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
    install(TARGETS wfmgen_cli DESTINATION bin)

    # CLI integration test (drives the built binary, checks byte output).
    add_test(NAME wfmgen_cli COMMAND ${CMAKE_COMMAND}
        -DEXE=$<TARGET_FILE:wfmgen_cli>
        -DWAVEGEN=$<TARGET_FILE:wavegen>
        -P ${CMAKE_SOURCE_DIR}/native/tests/wfmgen_cli_test.cmake)
endif()
