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
    ${CMAKE_SOURCE_DIR}/native/src/wfm/wfm_resolve.c
    # "prepare once, materialize many" stimulus engine — caches each source's
    # render (via the composer's own wfm_compose_build_synth) and re-weights it
    # per parameter point (SNR/gain/phase/enable/seed sweeps, bit-exact).
    ${CMAKE_SOURCE_DIR}/native/src/wfm/wfm_plan.c
    # straight-C bridge for the generated Synth's standalone generation
    # (jm composer source.generates) — builds a wfm_synth engine from a source.
    ${CMAKE_SOURCE_DIR}/native/src/wfm/wfm_synth_bridge.c)
target_include_directories(wfm_compose_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/wfm
    ${CMAKE_SOURCE_DIR}/vendor/cjson)
target_link_libraries(wfm_compose_core PUBLIC m)

add_executable(test_wfm_compose
    ${CMAKE_SOURCE_DIR}/native/tests/test_wfm_compose.c)
# the composer reuses synth, which pulls in lo/awgn/pn
target_link_libraries(test_wfm_compose PRIVATE
    wfm_compose_core wfm_dsp_core wfm_cjson wfm_synth_core lo_core awgn_core
    pn_core fir_core
    m)
target_include_directories(test_wfm_compose PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc ${CMAKE_SOURCE_DIR}/vendor/cjson)
add_test(NAME test_wfm_compose COMMAND test_wfm_compose)

# wfm_plan: Gate-0 (render("{}") ≡ full compose, memcmp==0) + per-axis
# gains/enable/snr/seed vs a full compose of the equivalently-modified spec.
add_executable(test_wfm_plan
    ${CMAKE_SOURCE_DIR}/native/tests/test_wfm_plan.c)
target_link_libraries(test_wfm_plan PRIVATE
    wfm_compose_core wfm_dsp_core wfm_cjson wfm_synth_core lo_core awgn_core
    pn_core fir_core
    m)
target_include_directories(test_wfm_plan PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc ${CMAKE_SOURCE_DIR}/vendor/cjson)
add_test(NAME test_wfm_plan COMMAND test_wfm_plan)

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

# ── NATS PUB sink — POSIX only (vendored nats.c + pthreads don't build on
#    MinGW; mirrors examples/c's streaming guard). The sink is independent of
#    the composer, so it links the stream layer, not synth. ─────────────────
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
        telemetry_core
        nats_vendor_static
        Threads::Threads m)
    target_include_directories(test_wfm_sink PRIVATE
        ${CMAKE_SOURCE_DIR}/native/inc)
    add_test(NAME test_wfm_sink COMMAND test_wfm_sink)
    set_tests_properties(test_wfm_sink PROPERTIES SKIP_RETURN_CODE 77)

    # ── wfmgen: the composer CLI (multi-segment / BLUE / SigMF / nats / record).
    #    Hand-written (a composer is not a single-object jm-app generator); thin
    #    glue over wfm_compose + wfm_writer + wfm_sink. POSIX-only (the
    #    vendored stream client).
    #
    #    The CLI body is the callable `doppler_wfmgen()` (wfmgen.c → the
    #    wfmgen_core OBJECT lib); the `wfmgen` binary is a one-line `main` shim
    #    (wfmgen_main.c) over it. Splitting the entry point off the body lets
    #    libdoppler archive the whole generator without a `main`-symbol clash in
    #    a downstream that links libdoppler.a alongside its own `main`. ────────
    add_library(wfmgen_core OBJECT ${CMAKE_SOURCE_DIR}/native/src/app/wfmgen.c)
    target_include_directories(wfmgen_core PUBLIC
        ${CMAKE_SOURCE_DIR}/native/inc
        ${CMAKE_SOURCE_DIR}/native/inc/wfm
        # configure-time doppler/version.h (DOPPLER_VERSION for --version)
        ${CMAKE_BINARY_DIR}/native/inc)

    # Weak no-op fallbacks for the wfm_stream_sink_* symbols wfmgen references, so
    # the pure-C core is self-contained and links everywhere with no special
    # flags (ELF + Mach-O). libdoppler_stream provides the strong overrides.
    add_library(wfm_sink_stub_core OBJECT
        ${CMAKE_SOURCE_DIR}/native/src/wfm/wfm_sink_stub.c)
    target_include_directories(wfm_sink_stub_core PUBLIC
        ${CMAKE_SOURCE_DIR}/native/inc
        ${CMAKE_SOURCE_DIR}/native/inc/wfm)

    add_executable(wfmgen_cli ${CMAKE_SOURCE_DIR}/native/src/app/wfmgen_main.c)
    set_target_properties(wfmgen_cli PROPERTIES OUTPUT_NAME wfmgen)
    target_link_libraries(wfmgen_cli PRIVATE
        wfmgen_core
        wfm_compose_core wfm_writer_core wfm_sink_core timing_core wfm_cjson
        wfm_dsp_core wfm_synth_core lo_core awgn_core pn_core fir_core
        $<TARGET_OBJECTS:stream_core_obj>
        telemetry_core
        nats_vendor_static
        Threads::Threads m)
    target_include_directories(wfmgen_cli PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
    install(TARGETS wfmgen_cli DESTINATION bin)

    # Archive the whole generator into libdoppler (.so + .a) so a downstream
    # linking the library can drive wfmgen in-process via
    # `doppler_wfmgen(argc, argv)` (wfm/wfmgen.h ships with the native/inc
    # install). The synth/lo/awgn/pn/fir/timing cores are already in libdoppler,
    # so only the wfmcompose objects are folded in here (+ ~132 KiB to the .a).
    # wfmgen stays in the pure-C core; its stream-sink path uses the weak-stub
    # seam (wfm_sink_stub.c here + the strong override in libdoppler_stream —
    # see native/inc/wfm/wfm_sink.h).  The stream-dependent objects
    # (wfm_sink_core, stream_core_obj) and the vendored nats.c are NOT folded
    # into libdoppler — they live in the optional libdoppler_stream component
    # (defined in the root CMakeLists), a separate module boundary rather
    # than a C++ isolation concern.
    foreach(_dt doppler_lib doppler_lib_static)
        target_sources(${_dt} PRIVATE
            $<TARGET_OBJECTS:wfmgen_core>
            $<TARGET_OBJECTS:wfm_sink_stub_core>
            $<TARGET_OBJECTS:wfm_compose_core>
            $<TARGET_OBJECTS:wfm_writer_core>
            $<TARGET_OBJECTS:wfm_dsp_core>
            $<TARGET_OBJECTS:wfm_cjson>)
    endforeach()

    # Bundle the wfmgen binary into the Python package so the wheel ships it as
    # package data (the `wfmgen` console-script is an os.execv shim over it —
    # see src/doppler/wfm/cli.py, docs/dev/wfmgen/api.md D2/D5). just-buildit's
    # wheel build copies the whole src/doppler tree, so _bin/wfmgen rides along.
    if(BUILD_PYTHON)
        add_custom_command(TARGET wfmgen_cli POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "${PYTHON_PACKAGE_DIR}/wfm/_bin"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:wfmgen_cli>"
                "${PYTHON_PACKAGE_DIR}/wfm/_bin/wfmgen"
            VERBATIM
            COMMENT "Bundle wfmgen CLI binary into the wheel package")
    endif()

    # CLI integration test (drives the built binary, checks byte output).
    add_test(NAME wfmgen_cli COMMAND ${CMAKE_COMMAND}
        -DEXE=$<TARGET_FILE:wfmgen_cli>
        -P ${CMAKE_SOURCE_DIR}/native/tests/wfmgen_cli_test.cmake)
endif()
