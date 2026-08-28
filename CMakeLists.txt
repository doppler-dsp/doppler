cmake_minimum_required(VERSION 3.16)
project(doppler VERSION 0.45.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Feature-test macros go on the COMPILE LINE, never in a header (doppler#986).
#
# glibc's features.h latches the feature set on its FIRST inclusion, so a
# `#define _GNU_SOURCE` inside a header is a no-op whenever anything reached
# libc before that header did. buffer.h carried one at its own line 51 and it
# was inert in exactly that way -- measured directly: after `#include
# <stdio.h>` then `#include "buffer/buffer.h"`, `__USE_GNU` is NOT defined.
#
# It looked like it worked because CMAKE_C_EXTENSIONS defaults ON, so this
# builds as -std=gnu99 and _DEFAULT_SOURCE already declares the syscall() and
# ftruncate() buffer.h calls. Under a strict -std=c99 the same file compiles
# or does not COMPILE ACCORDING TO INCLUDE ORDER ALONE, and native/inc is
# installed wholesale -- a downstream picks its own order and its own dialect.
#
# Defined for every target here, which is the only place the ordering is
# guaranteed. `scripts/check_installed_headers.py` keeps a header from
# growing one back.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(_GNU_SOURCE)
elseif(APPLE)
    add_compile_definitions(_DARWIN_C_SOURCE) # MAP_ANON
endif()

# doppler is pure C: every first-party source is C99 and the core libdoppler
# links -lm and -lpthread.  The vendored nats.c (the optional libdoppler_stream
# component's transport) is also pure C, so the whole build needs only a C
# compiler — the top-level project declares no CXX.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Always emit build/compile_commands.json so clangd/IDEs resolve include paths
# and feature-macro defines (Python, NumPy, vendored cJSON, _POSIX sources).
# Symlink it from the repo root once: `ln -s build/compile_commands.json .`
# (the file is gitignored). Without this a stale or absent DB makes clangd
# report bogus "file not found" / "unknown type" errors for those headers.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type — DEFAULT TO OPTIMISED, because empty means -O0, not -O2.
#
# CMake leaves CMAKE_BUILD_TYPE empty unless the caller sets one, and an empty
# type contributes NO -O flag at all: a bare `cmake -B build` — the command the
# README, CLAUDE.md and the C-API docs all document — builds the entire DSP
# library unoptimised. `make` (which passes -DCMAKE_BUILD_TYPE=$(BUILD_TYPE))
# and the release workflow were always explicit, so only the hand-typed
# configure was affected; it silently produced scalar, stack-spilled code and
# invalidated any benchmark run against it.
#
# Multi-config generators (Ninja Multi-Config, Visual Studio, Xcode) choose the
# configuration at BUILD time, so they are left alone.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
    message(STATUS
        "CMAKE_BUILD_TYPE was not set — defaulting to Release. "
        "Configure with -DCMAKE_BUILD_TYPE=Debug for an unoptimised build.")
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    Debug Release RelWithDebInfo MinSizeRel)

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
# dp_interrupt is core, not a component: the ring buffer and the file writer
# consult the same flag as the NATS wait, and both exist in a build with no
# stream component at all. An OBJECT library rather than a plain source
# because stream_core_obj is embedded as raw objects into targets that never
# link the core archive -- test_stream_nats_core, bench_stream, wfmgen -- so
# those need the objects too, not a link edge to a library they do not use.
add_library(dp_interrupt_obj OBJECT native/src/dp_interrupt.c)
target_include_directories(dp_interrupt_obj PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/native/inc>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/native/inc>)
set_target_properties(dp_interrupt_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(doppler_lib SHARED native/src/doppler_lib.c
    $<TARGET_OBJECTS:dp_interrupt_obj>)
add_library(doppler_lib_static STATIC native/src/doppler_lib.c
    $<TARGET_OBJECTS:dp_interrupt_obj>)
foreach(_t doppler_lib doppler_lib_static)
    target_include_directories(${_t} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/native/inc>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/native/inc>
        $<INSTALL_INTERFACE:include>)
    # The feature-test macro travels with the TARGET, so a downstream that
    # links doppler gets it on its own compile line without reading a note
    # (doppler#986). buffer.h is installed and calls syscall()/ftruncate();
    # it used to define _GNU_SOURCE itself, which is a no-op for any
    # translation unit that reached libc first -- so the requirement was
    # real, undocumented, and silently unmet. PUBLIC, not PRIVATE: it is the
    # CONSUMER's compile line that needs it, since the header is theirs to
    # include.
    target_compile_definitions(${_t} PUBLIC
        $<$<PLATFORM_ID:Linux>:_GNU_SOURCE>
        $<$<PLATFORM_ID:Darwin>:_DARWIN_C_SOURCE>)
    set_target_properties(${_t} PROPERTIES OUTPUT_NAME doppler)
endforeach()
# Export both libs under obvious target names: `doppler::doppler` (shared) and
# `doppler::doppler-static`. Both go through install(EXPORT) below, so CMake
# generates fully relocatable imported targets (no hand-rolled paths in the
# package config). The objects each archive folds in (pocketfft, wfmcompose)
# are baked into the .a/.so via target_sources — they are NOT export-time
# dependencies, so the static lib exports cleanly; its interface deps are m
# and Threads — pocketfft is pure C99 and the networking/stream layer is split
# into the optional libdoppler_stream component.
set_target_properties(doppler_lib PROPERTIES EXPORT_NAME doppler)
set_target_properties(doppler_lib_static PROPERTIES EXPORT_NAME doppler-static)
# pocketfft (pure C99) is compiled into fft_core, which is folded into both libs
# below with every other component — so no separate pocketfft target to wire here.
# The core is pure C and links -lm and -lpthread (the stream layer lives in
# the optional libdoppler_stream component below).
#
# Threads::Threads is PUBLIC on the ARCHIVE and must stay that way. A component
# that needs pthread carries it PUBLIC on its own target (ccsds_tm_core for
# rs.c's `pthread_once`, wfm_compose_core for dp_parallel), but every component
# is folded in here as $<TARGET_OBJECTS:...> — objects, not a link edge — and
# that drops the usage requirement on the floor. Nothing else re-stated it, so
# the archive's link interface carried no pthread and a consumer that pulled a
# pthread-using member out of libdoppler.a failed to link: on glibc < 2.34
# pthread is a separate library, so `native/examples/ccsds_link_demo` died on
# `undefined reference to pthread_once` in CI's Debian 10 job while every
# glibc >= 2.34 box linked it clean, pthread being folded into libc there.
find_package(Threads REQUIRED)
target_link_libraries(doppler_lib        PRIVATE m Threads::Threads)
target_link_libraries(doppler_lib_static PUBLIC  m Threads::Threads)

# The core ships weak no-op stubs for the wfm_stream_sink_* symbols wfmgen
# uses (native/src/wfm/wfm_sink_stub.c, folded in via the wfmcompose subdir),
# so the core is self-contained and needs no per-platform linker flags;
# linking libdoppler_stream supplies the strong overrides.

enable_testing()

# ── Components (add_subdirectory lines appended here by just-makeit) ──────────
add_subdirectory(native/src/hbdecim)
add_subdirectory(native/src/resamp)
add_subdirectory(native/src/wfmcompose)
add_subdirectory(native/src/ccsds_tm)
add_subdirectory(native/src/conv)
add_subdirectory(native/src/rs)
add_subdirectory(native/src/frame_meter)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:frame_meter_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:frame_meter_core>)
add_subdirectory(native/src/ber_meter)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:ber_meter_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:ber_meter_core>)
add_subdirectory(native/src/carrier_acq)
add_subdirectory(native/src/dsss_burst_receiver)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:dsss_burst_receiver_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:dsss_burst_receiver_core>)
add_subdirectory(native/src/async_dsss_receiver)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:carrier_acq_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:carrier_acq_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:async_dsss_receiver_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:async_dsss_receiver_core>)
add_subdirectory(native/src/dsss_receiver)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:dsss_receiver_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:dsss_receiver_core>)
add_subdirectory(native/src/burst_demod)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:burst_demod_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:burst_demod_core>)
add_subdirectory(native/src/ppe)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:ppe_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:ppe_core>)
add_subdirectory(native/src/burst_acq)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:burst_acq_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:burst_acq_core>)
add_subdirectory(native/src/acq)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:acq_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:acq_core>)
add_subdirectory(native/src/burst_despreader)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:burst_despreader_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:burst_despreader_core>)
add_subdirectory(native/src/despreader)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:despreader_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:despreader_core>)
add_subdirectory(native/src/interleaver)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:interleaver_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:interleaver_core>)
add_subdirectory(native/src/rs_codec)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:rs_codec_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:rs_codec_core>)
add_subdirectory(native/src/viterbi)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:viterbi_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:viterbi_core>)
add_subdirectory(native/src/conv_enc)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:conv_enc_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:conv_enc_core>)
add_subdirectory(native/src/mpsk_receiver)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:mpsk_receiver_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:mpsk_receiver_core>)
add_subdirectory(native/src/carrier_nda)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:carrier_nda_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:carrier_nda_core>)
add_subdirectory(native/src/carrier_mpsk)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:carrier_mpsk_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:carrier_mpsk_core>)
add_subdirectory(native/src/ratesync)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:ratesync_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:ratesync_core>)
add_subdirectory(native/src/symsync)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:symsync_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:symsync_core>)
add_subdirectory(native/src/dll)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:dll_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:dll_core>)
add_subdirectory(native/src/costas)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:costas_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:costas_core>)
add_subdirectory(native/src/loop_filter)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:loop_filter_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:loop_filter_core>)
add_subdirectory(native/src/interp_table)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:interp_table_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:interp_table_core>)
add_subdirectory(native/src/acc_q8)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:acc_q8_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:acc_q8_core>)
add_subdirectory(native/src/acc_q15)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:acc_q15_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:acc_q15_core>)
add_subdirectory(native/src/doppler_channel)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:doppler_channel_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:doppler_channel_core>)
add_subdirectory(native/src/syncword)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:syncword_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:syncword_core>)
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
add_subdirectory(native/src/ddcr)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:ddcr_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:ddcr_core>)
add_subdirectory(native/src/dp_tlm_capture)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:timing_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:timing_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:dp_tlm_capture_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:dp_tlm_capture_core>)
add_subdirectory(native/src/dp_tlm)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:dp_tlm_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:dp_tlm_core>)
add_subdirectory(native/src/dp_interrupt_guard)
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
add_subdirectory(native/src/frame)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:ccsds_tm_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:ccsds_tm_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:conv_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:conv_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:rs_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:rs_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:frame_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:frame_core>)
add_subdirectory(native/src/gold)
add_subdirectory(native/src/wfm_synth)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:resamp_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:resamp_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:wfm_dsp_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:wfm_dsp_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:wfm_frame_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:wfm_frame_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:gold_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:gold_core>)
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
# The lossless capture over a dp_tlm ring is hand-written C with no binding, so
# it is declared `no_generate` (alongside buffer and stream) rather than
# registered by hand: jm owns the component region below, and a hand-added
# add_subdirectory there is dropped on the next apply -- which is exactly how
# the capture's whole C test suite went unregistered once.

# ── Modules (add_subdirectory lines appended here by just-makeit) ─────────────
add_subdirectory(native/src/ber)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:ber_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:ber_core>)
add_subdirectory(native/src/acquire)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:acquire_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:acquire_core>)
add_subdirectory(native/src/dsss)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:dsss_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:dsss_core>)
add_subdirectory(native/src/coding)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:coding_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:coding_core>)
add_subdirectory(native/src/track)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:track_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:track_core>)
add_subdirectory(native/src/interp)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:interp_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:interp_core>)
add_subdirectory(native/src/arith)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:arith_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:arith_core>)
add_subdirectory(native/src/util)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:util_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:util_core>)
add_subdirectory(native/src/impairment)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:impairment_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:impairment_core>)
add_subdirectory(native/src/agc)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:agc_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:agc_core>)
add_subdirectory(native/src/mpsk)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:mpsk_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:mpsk_core>)
add_subdirectory(native/src/snr)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:snr_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:snr_core>)
add_subdirectory(native/src/detection)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:detection_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:detection_core>)
add_subdirectory(native/src/resample)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:resample_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:resample_core>)
add_subdirectory(native/src/analyzer)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:analyzer_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:analyzer_core>)
add_subdirectory(native/src/ddc)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:RateConverter_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:RateConverter_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:hbdecim_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:hbdecim_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:hbdecim_r2c_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:hbdecim_r2c_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:cic_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:cic_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:ddc_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:ddc_core>)
add_subdirectory(native/src/telemetry)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:telemetry_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:telemetry_core>)
add_subdirectory(native/src/interrupt)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:interrupt_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:interrupt_core>)
add_subdirectory(native/src/measure)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:measure_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:measure_core>)
add_subdirectory(native/src/spectral)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:spectral_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:spectral_core>)
add_subdirectory(native/src/delay)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:delay_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:delay_core>)
add_subdirectory(native/src/wfm_compose)
add_subdirectory(native/src/wfm)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:wfm_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:wfm_core>)
add_subdirectory(native/src/source)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:source_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:source_core>)
add_subdirectory(native/src/filter)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:filter_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:filter_core>)
add_subdirectory(native/src/accumulator)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:accumulator_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:accumulator_core>)
add_subdirectory(native/src/cvt)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:cvt_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:cvt_core>)
add_subdirectory(native/src/wfm_sink)
add_subdirectory(native/src/wfm_writer)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:wfm_writer_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:wfm_writer_core>)
add_subdirectory(native/src/sample_clock)
add_subdirectory(native/src/wfm_plan)
add_subdirectory(native/src/wfm_reader)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:dp_interrupt_guard_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:dp_interrupt_guard_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:wfm_cjson_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:wfm_cjson_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:wfm_keywords_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:wfm_keywords_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:wfm_draw_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:wfm_draw_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:wfm_reader_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:wfm_reader_core>)
add_subdirectory(native/src/buffer)
add_subdirectory(native/src/stream)

# ccsds_tm is a hand-owned c_dep, so jm knows its core exists (and says so
# when it reaches no library) but generates no wiring for it — that line is
# ours, and it goes HERE rather than beside
# `add_subdirectory(native/src/ccsds_tm)` because the block above is jm's
# splice region and an extra line inside it reads as drift.
#
# It has to reach both libraries: native/inc/ccsds_tm/*.h are installed
# headers, and every function they declare is out-of-line. Measured rather than
# assumed, which is the standard `status_allow` sets for the three exempted
# wfmcompose cores — `nm` over ccsds_tm_core's objects lists 13 defined `T`
# symbols against zero occurrences of the component's prefix in libdoppler.a,
# so before this
# line a C consumer could include the header and link none of it. Python was
# unaffected throughout, since the extension links each core directly, which
# is exactly why it went unnoticed until jm 0.62.0's wiring check.
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:ccsds_tm_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:ccsds_tm_core>)

# conv_core for the same reason, and it is the stronger case: native/inc/conv
# is an installed header whose every function is out-of-line, and `ccsds_tm`
# now reaches the encoder THROUGH it -- so without this line ccsds_tm's own
# symbols resolve inside the archive and the codec they call does not.
# jm 0.62.0's wiring check is what named it; the line is ours.
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:conv_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:conv_core>)

# rs_core, the outer code, on the same argument as conv_core: native/inc/rs
# is an installed header whose every function is out-of-line, and `ccsds_tm`
# holds only the CCSDS configuration of it -- so without this line
# ccsds_tm_rs_* resolve inside the archive and the field operations do not.
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:rs_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:rs_core>)

# The capture's block bound is claimed from a code audit of every emit site.
# This drives a REAL object -- carrier_nda, the only unconditional per-input
# emitter -- and measures what it actually emits, so the audit is checked
# rather than believed. It belongs to no single component (it spans dp_tlm,
# the capture and a whole receiver chain), so jm emits no target for it and it
# is registered here alongside the other cross-cutting suites. It previously
# lived in native/src/dp_tlm_capture/CMakeLists.txt, which jm now owns.
add_executable(test_tlm_bound_real native/tests/test_tlm_bound_real.c)
target_link_libraries(
  test_tlm_bound_real
  PRIVATE dp_tlm_capture_core dp_tlm_core carrier_nda_core lo_core
          loop_filter_core boxcar_core agc_core lockdet_core Threads::Threads m)
target_include_directories(test_tlm_bound_real
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_tlm_bound_real COMMAND test_tlm_bound_real)

# The detection module's free functions (marcum_q, det_* sizing) live in
# module-level TUs, so jm emits no C test target for them — register the
# module-level test here (per-object tests are jm-generated in their own
# component CMakeLists).
add_executable(test_detection_core native/tests/test_detection_core.c)
target_link_libraries(test_detection_core PRIVATE detection_core m)
target_include_directories(test_detection_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_detection_core COMMAND test_detection_core)

# util is function-only too: the EMA primitive and the saturate/square_clip
# pair are module-level, so their test is registered here for the same
# reason. It links util_core for the out-of-line definitions, though every
# assertion inlines the header copy — which is the point, since the header
# is what every C caller in the library actually compiles against.
add_executable(test_util_core native/tests/test_util_core.c)
target_link_libraries(test_util_core PRIVATE util_core m)
target_include_directories(test_util_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_util_core COMMAND test_util_core)

# mpsk is function-only in the same sense: the constellation map/demap pair
# and the differential variants are module-level TUs, and the decision rule
# itself (mpsk_slice, mpsk_phi0, the Gray helpers) is inline in the header
# with no TU at all. jm emits no C test target, so it is registered here.
# Links mpsk_core for the out-of-line array functions; the slicer assertions
# compile the header copy, which is what mpsk_receiver and mpsk_rx_loops.h
# actually decide through.
add_executable(test_mpsk_core native/tests/test_mpsk_core.c)
target_link_libraries(test_mpsk_core PRIVATE mpsk_core m)
target_include_directories(test_mpsk_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_mpsk_core COMMAND test_mpsk_core)

# snr is function-only as well — the two stateless estimators and their
# sliding-window forms are module-level TUs, so jm emits no C test target and
# the module had NO C test at all. Both estimators are called by
# test_async_dsss_receiver_core.c and by two harness headers, which exercises
# them as tools without pinning a single claim they make.
add_executable(test_snr_core native/tests/test_snr_core.c)
target_link_libraries(test_snr_core PRIVATE snr_core m)
target_include_directories(test_snr_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_snr_core COMMAND test_snr_core)

# ber is function-only in the same sense: the theory curves, the settled
# window and the self-referenced EVM are module-level TUs (ber_meter is the
# object, and has its own generated target). ber_evm_db is the harness's
# quality metric and was likewise never asserted directly.
add_executable(test_ber_core native/tests/test_ber_core.c)
target_link_libraries(test_ber_core PRIVATE ber_core detection_core mpsk_core m)
target_include_directories(test_ber_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_ber_core COMMAND test_ber_core)

# mpsk, ber and snr: function-only modules, so jm generates no bench target
# for them and `jm bench` cannot run one either (just-makeit#1023 -- run them
# by hand meanwhile). Same reason the EMA primitive's benchmark below is
# registered by hand. scripts/check_bench_coverage.py fails `make lint` if a
# tested component loses its benchmark, if a benchmark records nothing, or if
# it writes its JSON under a name no collector opens.
add_executable(bench_mpsk_core native/benchmarks/bench_mpsk_core.c)
target_link_libraries(bench_mpsk_core PRIVATE mpsk_core m)
target_include_directories(bench_mpsk_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)

add_executable(bench_ber_core native/benchmarks/bench_ber_core.c)
target_link_libraries(bench_ber_core PRIVATE ber_core detection_core mpsk_core m)
target_include_directories(bench_ber_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)

add_executable(bench_snr_core native/benchmarks/bench_snr_core.c)
target_link_libraries(bench_snr_core PRIVATE snr_core m)
target_include_directories(bench_snr_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)

# The EMA primitive's benchmark, registered here for the same reason its
# test is: util is function-only, so jm generates no bench target for it.
# It is a COMPARISON against the bodies the migrated call sites used to
# contain — the question is whether routing them through a shared inline
# cost anything, not how fast an EMA is.
add_executable(bench_util_core native/benchmarks/bench_util_core.c)
target_link_libraries(bench_util_core PRIVATE util_core m)
target_include_directories(bench_util_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)

# ── function-only modules: benchmarks jm does not generate (yet) ─────────
# arith, detection, filter, measure, resample, spectral and wfm carry their
# surface as free functions in <module>_core.c with no object of their own,
# so jm generates neither a bench source nor a bench target for them -- the
# same hole mpsk, ber, snr and util above sit in.
#
# DELETE THIS WHOLE BLOCK when the jm pin moves to a release containing
# just-buildit/just-makeit#1034, which generates these targets itself. The
# .c files stay either way: jm creates a bench source only when one is
# missing, so a filled-in benchmark survives the bump and an empty
# scaffold never lands on top of it.
#
# What happens if the block is NOT deleted depends on one more jm fix, so
# do not rely on a loud failure to remind you:
#   - without jm#1046, two add_executable calls share a name and the
#     configure dies -- loud, immediate, unmissable;
#   - with jm#1046 (in jm main, unreleased at the time of writing), jm
#     skips emitting a target the project already declares, per target,
#     and the build is silently fine with the workaround still in place.
# That fix exists BECAUSE gh-1034 collided with this very block, so the
# likely release contains both and the deletion is cleanup rather than a
# repair. Do it anyway: one place should own a target name.
#
# Written out one target at a time rather than looped, deliberately. A
# `foreach(_m ...)` is shorter and it makes the target name INVISIBLE:
# scripts/check_bench_coverage.py reads `add_executable(<name>` out of the
# CMake source to answer "can anything build this", and a name assembled
# from a loop variable is not there to be read. Measured -- the loop
# version failed rule 2 for all four of its targets while building them
# perfectly. Anything else grepping for a target has the same problem.
add_executable(bench_arith_core native/benchmarks/bench_arith_core.c)
target_link_libraries(bench_arith_core PRIVATE arith_core m)
target_include_directories(
  bench_arith_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                               ${CMAKE_SOURCE_DIR}/native/benchmarks)

add_executable(bench_detection_core native/benchmarks/bench_detection_core.c)
target_link_libraries(bench_detection_core PRIVATE detection_core m)
target_include_directories(
  bench_detection_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)

add_executable(bench_resample_core native/benchmarks/bench_resample_core.c)
target_link_libraries(bench_resample_core PRIVATE resample_core m)
target_include_directories(
  bench_resample_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                  ${CMAKE_SOURCE_DIR}/native/benchmarks)

add_executable(bench_spectral_core native/benchmarks/bench_spectral_core.c)
target_link_libraries(bench_spectral_core PRIVATE spectral_core m)
target_include_directories(
  bench_spectral_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                  ${CMAKE_SOURCE_DIR}/native/benchmarks)

# The three that are not self-contained, each for the reason its module
# composes rather than reimplements -- which is the doctrine working, and
# is why the link line is longer than the header suggests:
#   filter   design_lowpass is SIZED by resample's kaiser_num_taps and
#            SHAPED by spectral's kaiser_window
#   measure  measure_min_samples picks a window via spectral's
#            kaiser_beta_for_sidelobe
#   wfm      wfm_core is an OBJECT library, so every TU in it arrives
#            whether the benchmark calls it or not, and four of them
#            delegate: rrc_taps/dsss_spread to wfm_dsp_core,
#            wfm_awgn_amplitude to awgn_core, ccsds_asm_bits to
#            ccsds_tm_core. Not one of those is reimplemented here, which
#            is the whole reason the link line is longer than the header
add_executable(bench_measure_core native/benchmarks/bench_measure_core.c)
target_link_libraries(bench_measure_core PRIVATE measure_core spectral_core m)
target_include_directories(
  bench_measure_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                             ${CMAKE_SOURCE_DIR}/native/benchmarks)

add_executable(bench_wfm_core native/benchmarks/bench_wfm_core.c)
target_link_libraries(
  bench_wfm_core PRIVATE wfm_core wfm_dsp_core awgn_core ccsds_tm_core
                         conv_core rs_core m)
target_include_directories(
  bench_wfm_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                         ${CMAKE_SOURCE_DIR}/native/benchmarks)

add_executable(bench_filter_core native/benchmarks/bench_filter_core.c)
target_link_libraries(bench_filter_core PRIVATE filter_core spectral_core
                                                resample_core m)
target_include_directories(
  bench_filter_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/benchmarks)

# native/tests/dp_mf_test.h supplies the down-converter suites' stimulus AND
# their verdict. Its mf_evm_db() takes a MIN over strobe alignment -- the shape
# dp_ber_test.h calls "the historic footgun", legitimate here because the loop
# is open and the strobe phase is arbitrary, but exposed to the false-PASS the
# footgun names. The self-test measures that: a stream carrying the WRONG
# sequence must read badly at every alignment the search tries.
add_executable(test_dp_mf native/tests/test_dp_mf.c)
target_link_libraries(test_dp_mf PRIVATE m)
target_include_directories(test_dp_mf
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_compile_options(test_dp_mf PRIVATE -O2)
add_test(NAME test_dp_mf COMMAND test_dp_mf)

# native/tests/dp_dsss_test.h carries a KNOWN DEFECT in its own docstring
# (doppler#689): the noise is scaled for the wrong complex-Gaussian
# convention, so every capture is 3.01 dB quieter than it claims. It is
# deliberately unfixed -- correcting it makes an async BER sweep go
# non-monotonic, which is a receiver investigation rather than a constant --
# and until now it was held in place by prose alone. The self-test makes it a
# CHARACTERIZATION: the 3.01 dB is measured and asserted, so the magnitude is
# a fact, the level cannot drift further unnoticed, and a one-character "fix"
# turns the test red ON PURPOSE rather than quietly re-tuning two BER sweeps.
add_executable(test_dp_dsss native/tests/test_dp_dsss.c)
target_link_libraries(test_dp_dsss PRIVATE m)
target_include_directories(test_dp_dsss
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_compile_options(test_dp_dsss PRIVATE -O2)
add_test(NAME test_dp_dsss COMMAND test_dp_dsss)

# native/tests/dp_state_test.h is 12 lines and 31 test files call it -- the
# highest leverage per line in the family, and the only evidence most
# serializable objects have that their state interface works. Its macro pastes
# a PREFIX, so it cannot be tested against a real object without also testing
# that object; the self-test defines a fake one over the real dp_state.h
# envelope whose set_state can be switched between correct and three broken
# implementations, and reads whether the macro noticed from dp_test.h's
# counters rather than from an exit status.
#
# That is how the missing FIDELITY half was found: the macro asserted what
# set_state RETURNS and never that the restored object carries the state, so a
# set_state that validated the envelope and restored nothing passed at all 31
# sites.
# The interrupt primitive: three transports depend on it to be stoppable, so
# a regression is silent everywhere at once. Links the objects directly -- it
# tests the core primitive, not a component that embeds it.
add_executable(test_dp_interrupt native/tests/test_dp_interrupt.c
    $<TARGET_OBJECTS:dp_interrupt_obj>)
target_include_directories(test_dp_interrupt PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_dp_interrupt COMMAND test_dp_interrupt)

add_executable(test_dp_state native/tests/test_dp_state.c)
target_include_directories(test_dp_state
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_compile_options(test_dp_state PRIVATE -O2)
add_test(NAME test_dp_state COMMAND test_dp_state)

# native/tests/dp_tx_test.h is the harness STIMULUS SSOT, and docs/design/
# rx-test.md section 5.4 records it as the ONE place the stimulus rule cannot
# reach: check_stimulus_sources.py requires every test, validator and example
# to source stimulus from the library, and this file IS the test layer's
# stimulus. The gate that polices everyone else is structurally blind to it,
# and has nowhere to point -- so the conventions it would have checked are
# asserted here instead.
#
# The conventions are the product, not the loop: three C copies and four
# Python ones differed only in amplitude and level convention, and since a
# TED's slope goes as A^2 two of them were measuring loop bandwidths ~16x
# apart while both read as "the RRC BPSK test".
add_executable(test_dp_tx native/tests/test_dp_tx.c)
target_link_libraries(test_dp_tx PRIVATE pn_core wfm_dsp_core m)
target_include_directories(test_dp_tx
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_compile_options(test_dp_tx PRIVATE -O2)
add_test(NAME test_dp_tx COMMAND test_dp_tx)

# native/tests/dp_sym_test.h is the truth-free symbol-quality layer five test
# files score every receiver through. It is thin -- most entries forward to
# ber_core/snr_core -- so what is load-bearing is not its arithmetic but the
# NUMBERS its docstrings state: the -1.4/-7.0/-12.9 dB scatter floors, the
# EVM ~ -(Es/N0) anchor with no factor of two, and the 2*(5/bn_t + 5/bn_c)
# settling budget. Other files write fixed thresholds against those, so a
# drift keeps them passing and changes what they mean.
#
# The Monte-Carlo half is the part a closed form cannot establish: a stream at
# uniformly random phase is generated and measured against the floor, and then
# against the `< -12.0 dB` assertion the header records as live in
# the real receiver's every-M loop until 2026-07-27 -- which a fully scattered 8PSK
# stream passes.
add_executable(test_dp_sym native/tests/test_dp_sym.c)
target_link_libraries(test_dp_sym PRIVATE ber_core snr_core mpsk_core m)
target_include_directories(test_dp_sym
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_compile_options(test_dp_sym PRIVATE -O2)
add_test(NAME test_dp_sym COMMAND test_dp_sym)

# native/tests/dp_test.h is the assertion foundation 97 C test files include.
# It is the worst place in the tree for an untested thing to sit: a DP_CHECK
# that stops recording failures does not turn the suite red, it turns it
# GREEN, and ctest reports 100%. The header replaced 90 hand-rolled CHECK
# macros in six variants -- one with its condition inverted, twenty whose
# failure gate had drifted so 75 checks printed FAIL and still exited 0 --
# and nothing had been watching the replacement.
#
# The self-test observes dp_test.h's own counters rather than its exit status,
# which is how it can assert that a check FAILS without failing itself, and it
# captures stderr so a deliberate failure never puts a fake FAIL line in a
# passing test's log.
add_executable(test_dp_test native/tests/test_dp_test.c)
# `cabs` on the component-wise-vs-magnitude assertion. gcc at -O2 folds it and
# needs no libm; the coverage build is clang at -O0 and emits the call, so the
# omission was invisible to `make test` and red only in `make coverage`.
target_link_libraries(test_dp_test PRIVATE m)
target_include_directories(test_dp_test
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/tests)
target_compile_options(test_dp_test PRIVATE -O2)
add_test(NAME test_dp_test COMMAND test_dp_test)

# DP_TEST_END returns, so its three exit paths cannot be tested in-process.
# Each gets a process, and CTest asserts the status. `nothing` is the one that
# matters: the zero-assertion floor is the only guard between this suite and a
# test whose body never ran reading as a pass forever, and nothing had ever
# run a zero-assertion program to check that the floor fires.
add_executable(test_dp_test_end native/tests/test_dp_test_end.c)
target_include_directories(test_dp_test_end
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/tests)
target_compile_options(test_dp_test_end PRIVATE -O2)
add_test(NAME test_dp_test_end_nothing COMMAND test_dp_test_end nothing)
add_test(NAME test_dp_test_end_fail COMMAND test_dp_test_end fail)
add_test(NAME test_dp_test_end_pass COMMAND test_dp_test_end pass)
set_tests_properties(test_dp_test_end_nothing test_dp_test_end_fail
                     PROPERTIES WILL_FAIL TRUE)

# native/tests/dp_ber_test.h is the error-rate measurement harness every
# receiver test scores through -- the settling window, the Pfa-gated alignment,
# inverse binomial sampling and the exact confidence interval. It is header-
# only and belongs to no object, so its self-test is registered here. It is
# also the instrument the other tests trust, so it is tested harder than they
# are: Monte-Carlo interval coverage, false-alarm rate of the sync detector,
# and assertions that fail if a `min over (lag, rotation)` search is ever
# reintroduced into the scoring path.
add_executable(test_dp_ber native/tests/test_dp_ber.c)
target_link_libraries(test_dp_ber PRIVATE ber_core ber_meter_core detection_core snr_core m)
target_include_directories(test_dp_ber
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_compile_options(test_dp_ber PRIVATE -O2)
add_test(NAME test_dp_ber COMMAND test_dp_ber)

# native/tests/dp_rng_test.h is the suite's ONE random source: the data bits
# every modulated test transmits and the AWGN every receiver test is scored
# under. It is the single point where one edit moves every BER, EVM and
# lock-metric number at once -- plausibly, since noise that is slightly wrong
# still looks like noise. Header-only and owned by no object, so its self-test
# is registered here alongside the other two. It pins the integer streams
# exactly and the distributions statistically; the header says why the
# Gaussians cannot be pinned bit-for-bit across libm implementations.
add_executable(test_dp_rng native/tests/test_dp_rng.c)
target_link_libraries(test_dp_rng PRIVATE m)
target_include_directories(test_dp_rng
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/tests)
target_compile_options(test_dp_rng PRIVATE -O2)
add_test(NAME test_dp_rng COMMAND test_dp_rng)

# native/inc/dp_interleave.h is the block-interleaving permutation, shared by
# the WFM_STAGE_INTERLEAVE frame-stage kernel and the Interleaver object --
# header-only for the reason dp_crc16.h gives, so neither grows a link-line
# dependency for arithmetic. Owned by no object, so registered here. Links
# nothing.
add_executable(test_dp_interleave native/tests/test_dp_interleave.c)
target_include_directories(test_dp_interleave
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_compile_options(test_dp_interleave PRIVATE -O2)
add_test(NAME test_dp_interleave COMMAND test_dp_interleave)

# native/inc/dp_isotime.h formats filename-safe basic-format UTC timestamps.
# The format is NOT defined there -- it is just-bashit's `iso-8601-basic`, and
# code cannot be shared between a bash library and a C one, so this test holds
# the two together with golden vectors generated by the shell helper itself.
# Header-only and owned by no object, so it is registered here. It links
# nothing: the point of a header-only kernel is that no component grows a
# link-line dependency for a formatter.
add_executable(test_dp_isotime native/tests/test_dp_isotime.c)
target_include_directories(test_dp_isotime
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_compile_options(test_dp_isotime PRIVATE -O2)
add_test(NAME test_dp_isotime COMMAND test_dp_isotime)

# native/inc/wfm/wfm_time.h is the BLUE timecode <-> UNIX epoch conversion,
# shared by wfm_reader and wfm_writer (separate components, so header-only)
# and belonging to no object. Its test is mostly about the judgement around
# the offset rather than the offset: a zero timecode is UNSET, not 1950, and
# a pre-1970 capture must refuse rather than wrap into a future timestamp.
add_executable(test_wfm_time native/tests/test_wfm_time.c)
target_include_directories(test_wfm_time
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_compile_options(test_wfm_time PRIVATE -O2)
add_test(NAME test_wfm_time COMMAND test_wfm_time)

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

add_subdirectory(native/examples)

# ── Install ──────────────────────────────────────────────────────────────────

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS doppler_lib
    EXPORT doppler-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
# Static lib is a first-class export member → find_package gives consumers
# `doppler::doppler-static` (pure C, self-contained: -lm and -lpthread).
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
if(NOT WIN32)
    # One pkg-config name for the whole streaming link line:
    # `pkg-config --cflags --libs doppler_stream` (Requires: doppler).
    configure_file(cmake/doppler_stream.pc.in doppler_stream.pc @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/doppler_stream.pc"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

# ── App ───────────────────────────────────────────────────────────
# The single-shot `wavegen` is gone: a 1-segment `wfmgen` run is byte-for-byte
# identical to it (see native/tests/wfmgen_cli_test.cmake), so the composer
# `wfmgen` (native/src/wfmcompose/) is the one CLI. See docs/dev/wfmgen/api.md.
# ── App end ───────────────────────────────────────────────────────────
# ── Validation harnesses (hand-owned, Monte-Carlo vs theory) ─────────
add_subdirectory(native/validation)
# ── Validation end ──────────────────────────────────────────────────
