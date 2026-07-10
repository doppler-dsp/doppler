cmake_minimum_required(VERSION 3.16)
project(doppler VERSION 0.32.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# doppler is pure C: every first-party source is C99 and the core libdoppler
# links only -lm.  The vendored nats.c (the optional libdoppler_stream
# component's transport) is also pure C, so the whole build needs only a C
# compiler — the top-level project declares no CXX.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Always emit build/compile_commands.json so clangd/IDEs resolve include paths
# and feature-macro defines (Python, NumPy, vendored cJSON, _POSIX sources).
# Symlink it from the repo root once: `ln -s build/compile_commands.json .`
# (the file is gitignored). Without this a stale or absent DB makes clangd
# report bogus "file not found" / "unknown type" errors for those headers.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_SIMD "Enable SIMD / fast-math flags" ON)

# -march policy — DISTRIBUTION SAFETY IS THE DEFAULT.
#
# A distributed wheel must run on any user's CPU, not the build host's.
# -march=native bakes in whatever ISA the build machine happened to have
# (e.g. AVX-512 on a CI runner) and then raises SIGILL ("Illegal
# instruction") on older hardware that lacks it.  So the DEFAULT build
# targets a portable baseline, and -march=native is strictly OPT-IN:
#
#   default (any build, incl. every release/wheel path):
#       x86_64  -> -march=x86-64-v2  (SSE4.2, ~2009+; the manylinux baseline)
#       other   -> compiler default  (no -march; never the host's native ISA)
#   -DDOPPLER_NATIVE=ON (local dev/bench only; see `make blazing`):
#       -march=native  — MUST NOT reach a published wheel.
#
# Correctness no longer depends on any CI tool exporting an env var: the
# safe path is the default, speed is the thing you ask for.
option(DOPPLER_NATIVE
       "Tune for the build host CPU (-march=native). Local dev/bench ONLY — never for distributed wheels." OFF)
# Windows / MSVC is intentionally unsupported (signal-processing users on
# Windows run under WSL2, a VM, or a container), so only the GCC/Clang
# toolchains carry SIMD flags. The MSVC `/arch:AVX2` branch was never even
# exercised — Windows CI built with MinGW — and AVX2-by-default would SIGILL on
# pre-2013 CPUs anyway, the same portability trap the -march policy avoids.
if(ENABLE_SIMD AND NOT MSVC)
    if(DOPPLER_NATIVE)
        add_compile_options(-march=native)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            # Cap vectors at 256 bits even when -march=native enables AVX-512.
            # 256 is the x86 throughput sweet spot: it sidesteps AVX-512
            # frequency downclocking (Intel) and the double-pumped-512 penalty
            # (AMD Zen 4/5 mobile, e.g. Strix Point), where forcing 512-bit
            # vectors measured *slower* than SSE4.2 on complex-float DSP
            # kernels — while still using AVX-512's extra registers/encodings.
            add_compile_options(-mprefer-vector-width=256)
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        # x86-64-v2 (SSE4.2) is the preferred portable baseline, but the
        # microarch-level name only exists in GCC >= 11 / Clang >= 12.
        # Older toolchains (e.g. Debian 10's GCC 8) reject it, so probe
        # and fall back to the plain x86-64 baseline, which builds
        # everywhere and is equally portable.
        include(CheckCCompilerFlag)
        check_c_compiler_flag("-march=x86-64-v2" _doppler_have_v2)
        if(_doppler_have_v2)
            add_compile_options(-march=x86-64-v2)
        else()
            add_compile_options(-march=x86-64)
        endif()
    endif()
    # -fno-finite-math-only: bare -ffast-math makes GCC emit calls to
    # glibc's __*_finite aliases (e.g. __exp_finite), removed in
    # glibc 2.31 — a wheel built in the old-glibc manylinux image
    # then fails to load (undefined symbol) on modern Linux.
    # Disabling the finite-only assumption keeps every other
    # fast-math optimisation.
    add_compile_options(-ffast-math -fno-finite-math-only)
endif()

# Source-based code coverage (clang only) — driven by `make coverage`; see
# docs/dev/coverage.md. Instruments every first-party C object exactly once.
# Because the same <obj>_core OBJECT libraries are linked into the C tests, the
# Python .so, AND libdoppler.a (Rust), a single instrumented build lets all
# three harnesses emit .profraw that merge into one report attributed back to
# the hand-written _core.c. OFF by default and never in a release/wheel build.
option(DOPPLER_COVERAGE
       "Instrument with clang source-based coverage (dev/CI only)" OFF)
if(DOPPLER_COVERAGE)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR
            "DOPPLER_COVERAGE=ON needs clang (CMAKE_C_COMPILER_ID is "
            "${CMAKE_C_COMPILER_ID}); reconfigure with -DCMAKE_C_COMPILER=clang.")
    endif()
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate -fcoverage-mapping)
endif()

option(BUILD_PYTHON "Build Python C extensions" OFF)

if(BUILD_PYTHON)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module NumPy)
endif()

# Where built Python .so modules are placed (the importable package dir).
# Overridable so an instrumented coverage build can stage the .so into a throw-
# away package tree instead of clobbering the dev's working src/doppler/ .so.
set(PYTHON_PACKAGE_DIR "${CMAKE_SOURCE_DIR}/src/doppler"
    CACHE PATH "Destination for built Python extension modules")

# Stamp PROJECT_VERSION into doppler/version.h at configure time.
configure_file(
    native/inc/doppler/version.h.in
    ${CMAKE_BINARY_DIR}/native/inc/doppler/version.h
    @ONLY)

# Combined C library — shared + static, no Python dependency.
# Component OBJECT libraries are wired in via target_sources below.
add_library(doppler_lib SHARED native/src/doppler_lib.c)
add_library(doppler_lib_static STATIC native/src/doppler_lib.c)
foreach(_t doppler_lib doppler_lib_static)
    target_include_directories(${_t} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/native/inc>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/native/inc>
        $<INSTALL_INTERFACE:include>)
    set_target_properties(${_t} PROPERTIES OUTPUT_NAME doppler)
endforeach()
# Export both libs under obvious target names: `doppler::doppler` (shared) and
# `doppler::doppler-static`. Both go through install(EXPORT) below, so CMake
# generates fully relocatable imported targets (no hand-rolled paths in the
# package config). The objects each archive folds in (pocketfft, wfmcompose)
# are baked into the .a/.so via target_sources — they are NOT export-time
# dependencies, so the static lib exports cleanly; its only interface dep is
# m — pocketfft is pure C99 and the networking/stream layer is split into the
# optional libdoppler_stream component.
set_target_properties(doppler_lib PROPERTIES EXPORT_NAME doppler)
set_target_properties(doppler_lib_static PROPERTIES EXPORT_NAME doppler-static)
# pocketfft (pure C99) is compiled into fft_core, which is folded into both libs
# below with every other component — so no separate pocketfft target to wire here.
# The core is pure C and links only -lm (the stream layer lives in the
# optional libdoppler_stream component below). Threads is still located here
# for that component and the examples.
find_package(Threads REQUIRED)
target_link_libraries(doppler_lib        PRIVATE m)
target_link_libraries(doppler_lib_static PUBLIC  m)

# The core ships weak no-op stubs for the wfm_stream_sink_* symbols wfmgen
# uses (native/src/wfm/wfm_sink_stub.c, folded in via the wfmcompose subdir),
# so the core is self-contained and needs no per-platform linker flags;
# linking libdoppler_stream supplies the strong overrides.

enable_testing()

# ── Components (add_subdirectory lines appended here by just-makeit) ──────────
add_subdirectory(native/src/hbdecim)
add_subdirectory(native/src/resamp)
add_subdirectory(native/src/wfmcompose)
add_subdirectory(native/src/burst_demod)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:burst_demod_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:burst_demod_core>)
add_subdirectory(native/src/ppe)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:ppe_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:ppe_core>)
add_subdirectory(native/src/acq)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:acq_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:acq_core>)
add_subdirectory(native/src/burst_despreader)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:burst_despreader_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:burst_despreader_core>)
add_subdirectory(native/src/despreader)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:despreader_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:despreader_core>)
add_subdirectory(native/src/mpsk_receiver)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:mpsk_receiver_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:mpsk_receiver_core>)
add_subdirectory(native/src/carrier_nda)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:carrier_nda_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:carrier_nda_core>)
add_subdirectory(native/src/carrier_mpsk)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:carrier_mpsk_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:carrier_mpsk_core>)
add_subdirectory(native/src/symsync)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:symsync_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:symsync_core>)
add_subdirectory(native/src/dll)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:detection_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:detection_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:dll_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:dll_core>)
add_subdirectory(native/src/costas)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:costas_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:costas_core>)
add_subdirectory(native/src/loop_filter)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:loop_filter_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:loop_filter_core>)
add_subdirectory(native/src/acc_q8)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:acc_q8_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:acc_q8_core>)
add_subdirectory(native/src/acc_q15)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:acc_q15_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:acc_q15_core>)
add_subdirectory(native/src/lockdet)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:lockdet_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:lockdet_core>)
add_subdirectory(native/src/hbdecim_q15)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:hbdecim_q15_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:hbdecim_q15_core>)
add_subdirectory(native/src/farrow)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:farrow_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:farrow_core>)
add_subdirectory(native/src/RateConverter)
add_subdirectory(native/src/cic)
add_subdirectory(native/src/HalfbandDecimator)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:HalfbandDecimator_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:HalfbandDecimator_core>)
add_subdirectory(native/src/Resampler)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:Resampler_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:Resampler_core>)
add_subdirectory(native/src/specan)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:specan_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:specan_core>)
add_subdirectory(native/src/imdmeas)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:imdmeas_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:imdmeas_core>)
add_subdirectory(native/src/nprmeas)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:nprmeas_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:nprmeas_core>)
add_subdirectory(native/src/tonemeas)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:tonemeas_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:tonemeas_core>)
add_subdirectory(native/src/psd)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:spectral_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:spectral_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:psd_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:psd_core>)
add_subdirectory(native/src/detector2d)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:detector2d_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:detector2d_core>)
add_subdirectory(native/src/detector)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:detector_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:detector_core>)
add_subdirectory(native/src/corr2d)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:corr2d_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:corr2d_core>)
add_subdirectory(native/src/corr)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:corr_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:corr_core>)
add_subdirectory(native/src/fft2d)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:fft2d_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:fft2d_core>)
add_subdirectory(native/src/fft)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:fft_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:fft_core>)
add_subdirectory(native/src/wfm_synth)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:wfm_synth_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:wfm_synth_core>)
add_subdirectory(native/src/pn)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:pn_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:pn_core>)
add_subdirectory(native/src/awgn)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:awgn_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:awgn_core>)
add_subdirectory(native/src/lo)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:lo_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:lo_core>)
add_subdirectory(native/src/nco)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:nco_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:nco_core>)
add_subdirectory(native/src/boxcar)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:boxcar_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:boxcar_core>)
add_subdirectory(native/src/fir)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:fir_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:fir_core>)
add_subdirectory(native/src/acc_trace)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:acc_trace_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:acc_trace_core>)
add_subdirectory(native/src/acc_cf64)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:acc_cf64_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:acc_cf64_core>)
add_subdirectory(native/src/acc_f32)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:acc_f32_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:acc_f32_core>)
add_subdirectory(native/src/adc)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:adc_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:adc_core>)
add_subdirectory(native/src/uq15_to_f32)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:uq15_to_f32_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:uq15_to_f32_core>)
add_subdirectory(native/src/f32_to_uq15)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:f32_to_uq15_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:f32_to_uq15_core>)
add_subdirectory(native/src/i16u64_to_f32)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:i16u64_to_f32_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:i16u64_to_f32_core>)
add_subdirectory(native/src/i16u32_to_f32)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:i16u32_to_f32_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:i16u32_to_f32_core>)
add_subdirectory(native/src/f32_to_i16u64)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:f32_to_i16u64_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:f32_to_i16u64_core>)
add_subdirectory(native/src/f32_to_i16u32)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:f32_to_i16u32_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:f32_to_i16u32_core>)
add_subdirectory(native/src/i8_to_f32)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:i8_to_f32_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:i8_to_f32_core>)
add_subdirectory(native/src/i32_to_f32)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:i32_to_f32_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:i32_to_f32_core>)
add_subdirectory(native/src/i16_to_f32)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:i16_to_f32_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:i16_to_f32_core>)
add_subdirectory(native/src/f32_to_i16)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:f32_to_i16_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:f32_to_i16_core>)

# ── Modules (add_subdirectory lines appended here by just-makeit) ─────────────
add_subdirectory(native/src/dsss)
add_subdirectory(native/src/track)
add_subdirectory(native/src/arith)
add_subdirectory(native/src/util)
add_subdirectory(native/src/agc)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:telemetry_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:telemetry_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:agc_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:agc_core>)
add_subdirectory(native/src/mpsk)
add_subdirectory(native/src/snr)
add_subdirectory(native/src/detection)
add_subdirectory(native/src/resample)
add_subdirectory(native/src/analyzer)
add_subdirectory(native/src/ddc_fn)
add_subdirectory(native/src/ddc)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:RateConverter_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:RateConverter_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:resamp_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:resamp_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:hbdecim_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:hbdecim_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:hbdecim_r2c_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:hbdecim_r2c_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:cic_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:cic_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:resample_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:resample_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:ddc_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:ddc_core>)
add_subdirectory(native/src/measure)
add_subdirectory(native/src/spectral)
add_subdirectory(native/src/delay)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:delay_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:delay_core>)
add_subdirectory(native/src/wfm_compose)
add_subdirectory(native/src/wfm)
add_subdirectory(native/src/source)
add_subdirectory(native/src/filter)
add_subdirectory(native/src/accumulator)
add_subdirectory(native/src/cvt)
add_subdirectory(native/src/wfm_sink)
add_subdirectory(native/src/wfm_writer)
add_subdirectory(native/src/sample_clock)
add_subdirectory(native/src/wfm_plan)
add_subdirectory(native/src/wfm_reader)
add_subdirectory(native/src/buffer)
add_subdirectory(native/src/telemetry)
add_subdirectory(native/src/stream)
add_subdirectory(native/src/wfmcompose_py)
# The detection module's free functions (marcum_q, det_* sizing) live in
# module-level TUs, so jm emits no C test target for them — register the
# module-level test here (per-object tests are jm-generated in their own
# component CMakeLists).
add_executable(test_detection_core native/tests/test_detection_core.c)
target_link_libraries(test_detection_core PRIVATE detection_core m)
target_include_directories(test_detection_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_detection_core COMMAND test_detection_core)

# ── Hand-managed modules (not controlled by just-makeit) ─────────────────────

# ── Vendored nats.c (static, PIC) ────────────────────────────────────────────
# stream.so statically links libnats so it has no runtime dep on a libnats.so.
# TLS and NATS Streaming (STAN, which pulls protobuf-c) are OFF; JetStream and
# KV (js.c/jsm.c/kv.c) stay in.  nats.c is pure C, so it lands only in the
# libdoppler_stream tier and adds no C++ anywhere in the build.
set(_NATS_SRC "${CMAKE_SOURCE_DIR}/vendor/nats.c")
set(_NATS_BLD "${CMAKE_CURRENT_BINARY_DIR}/libnats-vendor")
set(_NATS_LIB "${_NATS_BLD}/lib/libnats_static.a")
add_custom_command(OUTPUT "${_NATS_LIB}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_NATS_BLD}"
    COMMAND ${CMAKE_COMMAND} -B "${_NATS_BLD}" -S "${_NATS_SRC}"
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DNATS_BUILD_WITH_TLS=OFF
            -DNATS_BUILD_STREAMING=OFF
            -DNATS_BUILD_LIB_SHARED=OFF
            -DNATS_BUILD_LIB_STATIC=ON
            -DNATS_BUILD_EXAMPLES=OFF
            -DNATS_BUILD_USE_SODIUM=OFF
            -DBUILD_TESTING=OFF
    COMMAND ${CMAKE_COMMAND} --build "${_NATS_BLD}" --parallel
    COMMENT "Building vendored nats.c (static, PIC; TLS/STAN off)" VERBATIM)
add_custom_target(libnats_vendor DEPENDS "${_NATS_LIB}")
add_library(nats_vendor_static STATIC IMPORTED GLOBAL)
set_target_properties(nats_vendor_static PROPERTIES
    IMPORTED_LOCATION             "${_NATS_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${_NATS_SRC}/src")
add_dependencies(nats_vendor_static libnats_vendor)

# ── Optional stream component (libdoppler_stream) ────────────────────────────
# The networking layer is split OUT of the core so a consumer that doesn't
# need streaming isn't forced to pull in nats.c/pthreads.  libdoppler_stream
# bundles the stream wire layer (stream_core) and the wfm stream sink
# (wfm_sink_core) and links the vendored nats.c.  It provides the strong
# wfm_stream_sink_* definitions that satisfy the core's weak seam
# (native/inc/wfm/wfm_sink.h), so a C consumer that wants `--output nats://`
# (or the dp_pub_*/dp_sub_* wire layer) links `doppler::stream` alongside
# `doppler::doppler[-static]`.  POSIX-only, like the vendored nats.c build.
# nats.c stays statically embedded — no runtime libnats.so in either the
# shared or static form.  Pure C: this component carries no C++ runtime.
if(NOT WIN32)
    add_library(doppler_stream        SHARED
        $<TARGET_OBJECTS:stream_core_obj> $<TARGET_OBJECTS:wfm_sink_core>)
    add_library(doppler_stream_static STATIC
        $<TARGET_OBJECTS:stream_core_obj> $<TARGET_OBJECTS:wfm_sink_core>)
    foreach(_s doppler_stream doppler_stream_static)
        set_target_properties(${_s} PROPERTIES OUTPUT_NAME doppler_stream)
        target_include_directories(${_s} PUBLIC
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/native/inc>
            $<INSTALL_INTERFACE:include>)
    endforeach()
    set_target_properties(doppler_stream        PROPERTIES EXPORT_NAME stream)
    set_target_properties(doppler_stream_static PROPERTIES EXPORT_NAME stream-static)
    # tlm_sink.c (in stream_core_obj) calls dp_tlm_read from the core's
    # telemetry_core — the component's one cross-lib symbol. Linking the
    # core keeps the SHARED form self-consistent (Mach-O's ld64 rejects
    # undefined dylib symbols); consumers already link both per the
    # contract above. The STATIC archive needs nothing (archives don't
    # resolve at ar time).
    target_link_libraries(doppler_stream
        PRIVATE nats_vendor_static doppler_lib
        PUBLIC  Threads::Threads m)
    target_link_libraries(doppler_stream_static PUBLIC Threads::Threads m)
    # The static form can't embed an archive through link rules, so fold the
    # vendored libnats.a objects in (the ar-merge the core used to use) —
    # leaving the static consumer with `-ldoppler_stream` + the C runtime,
    # no -lnats.
    add_dependencies(doppler_stream        libnats_vendor)
    add_dependencies(doppler_stream_static libnats_vendor)
    add_custom_command(TARGET doppler_stream_static POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDEST=$<TARGET_FILE:doppler_stream_static>
            -DSRC=${_NATS_LIB}
            -DAR=${CMAKE_AR}
            -DRANLIB=${CMAKE_RANLIB}
            -P ${CMAKE_SOURCE_DIR}/cmake/merge_static_libs.cmake
        COMMENT "Folding libnats.a into libdoppler_stream.a (self-contained)"
        VERBATIM)
endif()

add_subdirectory(examples/c)

# ── Install ──────────────────────────────────────────────────────────────────

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS doppler_lib
    EXPORT doppler-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
# Static lib is a first-class export member → find_package gives consumers
# `doppler::doppler-static` (pure C, self-contained: only -lm).
install(TARGETS doppler_lib_static
    EXPORT doppler-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Optional stream component → `doppler::stream` / `doppler::stream-static`.
# Link it only if you need the dp_pub_*/dp_sub_*/etc. NATS wire layer.
if(NOT WIN32)
    install(TARGETS doppler_stream doppler_stream_static
        EXPORT doppler-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

install(DIRECTORY ${CMAKE_SOURCE_DIR}/native/inc/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
    PATTERN "pyex_common.h" EXCLUDE)

install(EXPORT doppler-targets
    FILE doppler-targets.cmake
    NAMESPACE doppler::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/doppler)

# Build-tree export (#380): doppler-config.cmake is generated into the build
# tree below and includes doppler-targets.cmake from its own directory, but
# install(EXPORT) only materialises that file at install time — leaving the
# build tree a false-positive package prefix (config present, targets
# missing; any find_package pointed at build/ failed at configure). Emit the
# same export at generate time so `-DDoppler_DIR=<build>` works uninstalled.
export(EXPORT doppler-targets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/doppler-targets.cmake"
    NAMESPACE doppler::)

configure_package_config_file(
    cmake/doppler-config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/doppler-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/doppler)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/doppler-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/doppler-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/doppler-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/doppler)

configure_file(cmake/doppler.pc.in doppler.pc @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/doppler.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# ── App ───────────────────────────────────────────────────────────
# The single-shot `wavegen` is gone: a 1-segment `wfmgen` run is byte-for-byte
# identical to it (see native/tests/wfmgen_cli_test.cmake), so the composer
# `wfmgen` (native/src/wfmcompose/) is the one CLI. See docs/dev/wfmgen/api.md.
# ── App end ───────────────────────────────────────────────────────────
# ── Validation harnesses (hand-owned, Monte-Carlo vs theory) ─────────
add_subdirectory(native/validation)
# ── Validation end ──────────────────────────────────────────────────
