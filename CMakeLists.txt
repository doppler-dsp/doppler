cmake_minimum_required(VERSION 3.16)
project(doppler VERSION 0.3.1 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ENABLE_SIMD "Enable SIMD flags (-march=native -ffast-math / /arch:AVX2 /fp:fast)" ON)
if(ENABLE_SIMD)
    if(MSVC)
        add_compile_options(/arch:AVX2 /fp:fast)
    else()
        add_compile_options(-march=native -ffast-math)
    endif()
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
add_subdirectory(native/src/HalfbandDecimator)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:HalfbandDecimator_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:HalfbandDecimator_core>)
add_subdirectory(native/src/Resampler)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:Resampler_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:Resampler_core>)
add_subdirectory(native/src/fft2d)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:fft2d_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:fft2d_core>)
add_subdirectory(native/src/fft)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:fft_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:fft_core>)
add_subdirectory(native/src/lo)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:lo_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:lo_core>)
add_subdirectory(native/src/nco)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:nco_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:nco_core>)
add_subdirectory(native/src/fir)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:fir_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:fir_core>)
add_subdirectory(native/src/acc_cf64)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:acc_cf64_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:acc_cf64_core>)
add_subdirectory(native/src/acc_f32)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:acc_f32_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:acc_f32_core>)
# ── Modules (add_subdirectory lines appended here by just-makeit) ─────────────
add_subdirectory(native/src/spectral)
add_subdirectory(native/src/delay)
add_subdirectory(native/src/source)
add_subdirectory(native/src/filter)
add_subdirectory(native/src/accumulator)
add_subdirectory(native/src/buffer)
add_subdirectory(native/src/resamp)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:resamp_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:resamp_core>)
add_subdirectory(native/src/hbdecim)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:hbdecim_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:hbdecim_core>)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:hbdecim_r2c_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:hbdecim_r2c_core>)
add_subdirectory(native/src/resample)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:resample_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:resample_core>)
add_subdirectory(native/src/ddc)
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:ddc_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:ddc_core>)

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

add_subdirectory(native/src/stream)
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
