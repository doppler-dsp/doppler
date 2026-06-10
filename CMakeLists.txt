cmake_minimum_required(VERSION 3.16)
project(doppler VERSION 0.10.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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

option(BUILD_PYTHON "Build Python C extensions" OFF)

if(BUILD_PYTHON)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module NumPy)
endif()

set(PYTHON_PACKAGE_DIR "${CMAKE_SOURCE_DIR}/src/doppler")

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
# pocketfft_cxx is an OBJECT library — link it directly so its compiled units
# are embedded in the shared lib without appearing in the install export set.
# The static lib gets the same objects via target_sources so libdoppler.a is
# also self-contained; stdc++ and Threads propagate to consumers via PUBLIC.
find_package(Threads REQUIRED)
target_link_libraries(doppler_lib PRIVATE pocketfft_cxx m)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:pocketfft_cxx>)
target_link_libraries(doppler_lib_static PUBLIC stdc++ Threads::Threads m)

enable_testing()

# ── Components (add_subdirectory lines appended here by just-makeit) ──────────
add_subdirectory(native/src/hbdecim)
add_subdirectory(native/src/resamp)
add_subdirectory(native/src/wfmcompose)
add_subdirectory(native/src/acc_q8)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:acc_q8_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:acc_q8_core>)
add_subdirectory(native/src/acc_q15)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:acc_q15_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:acc_q15_core>)
add_subdirectory(native/src/RateConverter)
add_subdirectory(native/src/cic)
add_subdirectory(native/src/HalfbandDecimator)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:HalfbandDecimator_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:HalfbandDecimator_core>)
add_subdirectory(native/src/Resampler)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:Resampler_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:Resampler_core>)
add_subdirectory(native/src/ddcr)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:ddcr_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:ddcr_core>)
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
add_subdirectory(native/src/synth)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:synth_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:synth_core>)
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
add_subdirectory(native/src/hbdecim_q15)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:hbdecim_q15_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:hbdecim_q15_core>)
add_subdirectory(native/src/fir)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:fir_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:fir_core>)
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
add_subdirectory(native/src/arith)
add_subdirectory(native/src/util)
add_subdirectory(native/src/agc)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:agc_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:agc_core>)
add_subdirectory(native/src/detection)
add_subdirectory(native/src/resample)
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
add_subdirectory(native/src/spectral)
add_subdirectory(native/src/delay)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:delay_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:delay_core>)
add_subdirectory(native/src/wfmgen)
add_subdirectory(native/src/source)
add_subdirectory(native/src/filter)
add_subdirectory(native/src/accumulator)
add_subdirectory(native/src/cvt)
add_subdirectory(native/src/buffer)
add_subdirectory(native/src/stream)
add_subdirectory(native/src/ddc_fn)
add_subdirectory(native/src/wfmcompose_py)

# ── Hand-managed modules (not controlled by just-makeit) ─────────────────────

# ── Vendored libzmq (static, PIC) ────────────────────────────────────────────
# stream.so statically links libzmq so it has no runtime dep on libzmq.so.5.
set(_ZMQ_SRC "${CMAKE_SOURCE_DIR}/vendor/libzmq")
set(_ZMQ_BLD "${CMAKE_CURRENT_BINARY_DIR}/libzmq-vendor")
set(_ZMQ_LIB "${_ZMQ_BLD}/lib/libzmq.a")
add_custom_command(OUTPUT "${_ZMQ_LIB}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_ZMQ_BLD}"
    COMMAND ${CMAKE_COMMAND} -B "${_ZMQ_BLD}" -S "${_ZMQ_SRC}"
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DBUILD_SHARED=OFF -DBUILD_STATIC=ON
            -DWITH_DOCS=OFF -DZMQ_BUILD_TESTS=OFF
            -DCMAKE_POLICY_VERSION_MINIMUM=3.5
            "-DCMAKE_CXX_FLAGS="
    COMMAND ${CMAKE_COMMAND} -B "${_ZMQ_BLD}" -S "${_ZMQ_SRC}"
            "-DCMAKE_CXX_FLAGS=-I${_ZMQ_BLD} -I${_ZMQ_SRC}/src\
 -include precompiled.hpp -include macros.hpp -include command.hpp\
 -Wno-missing-field-initializers -Wno-missing-braces"
    COMMAND ${CMAKE_COMMAND} --build "${_ZMQ_BLD}" --parallel
    COMMENT "Building vendored libzmq (static, PIC)" VERBATIM)
add_custom_target(libzmq_vendor DEPENDS "${_ZMQ_LIB}")
add_library(zmq_vendor_static STATIC IMPORTED GLOBAL)
set_target_properties(zmq_vendor_static PROPERTIES
    IMPORTED_LOCATION             "${_ZMQ_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${_ZMQ_SRC}/include")
add_dependencies(zmq_vendor_static libzmq_vendor)

add_subdirectory(examples/c)

# ── Install ──────────────────────────────────────────────────────────────────

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS doppler_lib
    EXPORT doppler-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
# Static lib installed without CMake package integration — no pocketfft_cxx
# in the export set.  Consumers wanting static linking find libdoppler.a
# via find_library() and link -lstdc++ -lpthread -lm alongside it.
install(TARGETS doppler_lib_static
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY ${CMAKE_SOURCE_DIR}/native/inc/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
    PATTERN "pyex_common.h" EXCLUDE)

install(EXPORT doppler-targets
    FILE doppler-targets.cmake
    NAMESPACE doppler::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/doppler)

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
add_executable(wavegen native/src/app/wavegen.c)
target_link_libraries(wavegen PRIVATE synth_core lo_core awgn_core pn_core $<$<NOT:$<PLATFORM_ID:Windows>>:m>)
install(TARGETS wavegen DESTINATION bin)
# ── App end ───────────────────────────────────────────────────────────
