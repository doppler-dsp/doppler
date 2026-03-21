
# ------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------
# FFT backend selection (FFTW = default, pocketfft = MIT-only)
#
#   cmake -DUSE_FFTW=ON  ...   FFTW3 backend (fast, but GPL binary)
#   cmake -DUSE_FFTW=OFF ...   pocketfft backend (MIT-only binary)
# ------------------------------------------------------------
option(USE_FFTW "Use FFTW3 backend (default). OFF selects pocketfft (MIT)." ON)

set(FFT_BACKEND_NAME     "")
set(FFT_BACKEND_SOURCES  "")
set(FFT_BACKEND_LIBS     "")
set(FFT_BACKEND_INCLUDES "")

if(USE_FFTW)
    message(STATUS "FFT backend: FFTW (distributed binary is GPL-licensed)")
    set(FFT_BACKEND_NAME "FFTW")

    find_library(FFTW3_LIB         fftw3         REQUIRED)
    find_path   (FFTW3_INCLUDE_DIR fftw3.h       REQUIRED)
    # fftw3_threads is a separate .so on Linux; on Windows/MSYS2 thread
    # support is compiled into the main fftw3 library — make it optional.
    find_library(FFTW3_THREADS_LIB fftw3_threads)

    message(STATUS "  FFTW include: ${FFTW3_INCLUDE_DIR}")
    message(STATUS "  FFTW lib:     ${FFTW3_LIB}")
    if(FFTW3_THREADS_LIB)
        message(STATUS "  FFTW threads: ${FFTW3_THREADS_LIB}")
    else()
        message(STATUS "  FFTW threads: (not found — using fftw3 built-in)")
    endif()

    set(FFT_BACKEND_SOURCES  src/fft.c)
    set(FFT_BACKEND_LIBS     ${FFTW3_LIB} $<$<BOOL:${FFTW3_THREADS_LIB}>:${FFTW3_THREADS_LIB}> m)
    set(FFT_BACKEND_INCLUDES ${FFTW3_INCLUDE_DIR})

else()
    message(STATUS "FFT backend: pocketfft (MIT-only binary)")
    set(FFT_BACKEND_NAME "POCKETFFT")

    enable_language(CXX)

    set(FFT_BACKEND_SOURCES
        src/fft_pocketfft.c
        src/pocketfft.cc
    )

    set(FFT_BACKEND_LIBS m)

endif()

message(STATUS "FFT backend: ${FFT_BACKEND_NAME}")

# ------------------------------------------------------------
# ZMQ discovery for streaming interface
# ------------------------------------------------------------
find_library(ZMQ_LIB zmq REQUIRED)
find_path(ZMQ_INCLUDE_DIR zmq.h REQUIRED)

message(STATUS "ZMQ include: ${ZMQ_INCLUDE_DIR}")
message(STATUS "ZMQ lib: ${ZMQ_LIB}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ------------------------------------------------------------
# Source list shared by both library variants
# ------------------------------------------------------------
set(DOPPLER_SOURCES
    src/core.c
    src/fir.c
    src/simd.c
    src/stream.c
    ${FFT_BACKEND_SOURCES}
)

# ------------------------------------------------------------
# Shared library — used by the Python extension and installed
# for consumers that want to link dynamically.
# ------------------------------------------------------------
add_library(doppler SHARED ${DOPPLER_SOURCES})

target_include_directories(doppler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${FFT_BACKEND_INCLUDES}
        ${ZMQ_INCLUDE_DIR}
)

target_link_libraries(doppler
    PRIVATE
        ${FFT_BACKEND_LIBS}
        ${ZMQ_LIB}
        m
)

target_compile_definitions(doppler
    PRIVATE
        FFT_BACKEND_${FFT_BACKEND_NAME}=1
)

# ------------------------------------------------------------
# Static library — linked into the example/test executables so
# they are standalone (no libdoppler.so needed at runtime).
# libzmq, libfftw3, libm, libpthread are still system-dynamic.
# ------------------------------------------------------------
add_library(doppler_static STATIC ${DOPPLER_SOURCES})

target_include_directories(doppler_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${FFT_BACKEND_INCLUDES}
        ${ZMQ_INCLUDE_DIR}
)

target_link_libraries(doppler_static
    PRIVATE
        ${FFT_BACKEND_LIBS}
        ${ZMQ_LIB}
        m
)

target_compile_definitions(doppler_static
    PRIVATE
        FFT_BACKEND_${FFT_BACKEND_NAME}=1
)

# Give it the conventional lib name without the "_static" suffix in the file
set_target_properties(doppler_static PROPERTIES OUTPUT_NAME doppler)


# ------------------------------------------------------------
# Install rules (Unified library)
# ------------------------------------------------------------
install(TARGETS doppler doppler_static
    EXPORT dopplerTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    include/doppler.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES
    include/dp/buffer.h
    include/dp/core.h
    include/dp/fft.h
    include/dp/fir.h
    include/dp/simd.h
    include/dp/stream.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dp
)

# cmake package config
install(EXPORT dopplerTargets
    FILE dopplerTargets.cmake
    NAMESPACE doppler::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/doppler
)

configure_package_config_file(
    cmake/DopplerConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/DopplerConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/doppler
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/DopplerConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/DopplerConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/DopplerConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/doppler
)

# pkg-config
configure_file(
    cmake/doppler.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/doppler.pc
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/doppler.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)


# ------------------------------------------------------------
# Examples & Tests
# ------------------------------------------------------------

# Core DSP Demos
add_executable(fft_demo examples/fft_demo.c)
target_link_libraries(fft_demo PRIVATE doppler_static m pthread)

add_executable(fir_demo examples/fir_demo.c)
target_link_libraries(fir_demo PRIVATE doppler_static m)

add_executable(simd_demo examples/simd_demo.c)
target_link_libraries(simd_demo PRIVATE doppler_static m)

add_executable(bench_fft_c bench/bench_fft.c)
target_link_libraries(bench_fft_c PRIVATE doppler_static m pthread)

add_executable(bench_fir_c bench/bench_fir.c)
target_link_libraries(bench_fir_c PRIVATE doppler_static m)

add_executable(bench_buffer_c bench/bench_buffer.c)
target_link_libraries(bench_buffer_c PRIVATE doppler_static m pthread)

add_executable(fft_testbench tests/fft_testbench.c)
target_link_libraries(fft_testbench PRIVATE doppler_static m pthread)

# Streaming Demos (Unified API)
add_executable(transmitter examples/transmitter.c)
target_include_directories(transmitter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(transmitter PRIVATE doppler_static m)

add_executable(receiver examples/receiver.c)
target_include_directories(receiver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(receiver PRIVATE doppler_static m)

add_executable(spectrum_analyzer examples/spectrum_analyzer.c)
target_include_directories(spectrum_analyzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(spectrum_analyzer PRIVATE doppler_static m)

add_executable(pipeline_demo examples/pipeline_demo.c)
target_include_directories(pipeline_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pipeline_demo PRIVATE doppler_static m)

# Streaming unit tests
add_executable(test_stream tests/test_stream.c)
target_include_directories(test_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(test_stream PRIVATE doppler_static m pthread)

# FIR unit tests
add_executable(test_fir tests/test_fir.c)
target_link_libraries(test_fir PRIVATE doppler_static m)

# ------------------------------------------------------------
# CTest registration
# ------------------------------------------------------------
enable_testing()

add_test(NAME streaming_unit_tests COMMAND test_stream)
add_test(NAME fft_testbench        COMMAND fft_testbench)
add_test(NAME fir_unit_tests       COMMAND test_fir)

# ------------------------------------------------------------
# Windows: stage runtime DLLs next to the executables so they
# can run without modifying PATH (MSYS2 shell or Explorer).
# Runs at cmake configure time; re-run cmake after pacman -Syu.
# ------------------------------------------------------------
if(WIN32)
    set(_stage_dlls)

    find_file(ZMQ_DLL NAMES libzmq.dll zmq.dll
              HINTS ${CMAKE_PREFIX_PATH}
              PATH_SUFFIXES bin
              NO_DEFAULT_PATH)
    if(ZMQ_DLL)
        list(APPEND _stage_dlls "${ZMQ_DLL}")
        message(STATUS "Staging DLL: ${ZMQ_DLL}")
    else()
        message(WARNING
            "libzmq.dll not found; pipeline_demo/transmitter will "
            "need libzmq.dll on PATH at runtime.")
    endif()

    if(USE_FFTW)
        find_file(FFTW3_DLL NAMES libfftw3-3.dll fftw3-3.dll fftw3.dll
                  HINTS ${CMAKE_PREFIX_PATH}
                  PATH_SUFFIXES bin
                  NO_DEFAULT_PATH)
        if(FFTW3_DLL)
            list(APPEND _stage_dlls "${FFTW3_DLL}")
            message(STATUS "Staging DLL: ${FFTW3_DLL}")
        endif()

        find_file(FFTW3_THREADS_DLL NAMES libfftw3_threads-3.dll
                  HINTS ${CMAKE_PREFIX_PATH}
                  PATH_SUFFIXES bin
                  NO_DEFAULT_PATH)
        if(FFTW3_THREADS_DLL)
            list(APPEND _stage_dlls "${FFTW3_THREADS_DLL}")
            message(STATUS "Staging DLL: ${FFTW3_THREADS_DLL}")
        endif()
    endif()

    if(_stage_dlls)
        file(COPY ${_stage_dlls} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
endif()
