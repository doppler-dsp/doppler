cmake_minimum_required(VERSION 3.16)

# ── Standalone DSP examples (no streaming) ────────────────────────────────────
# Link against the static library so examples are self-contained and have
# no runtime dependency on libdoppler.so / doppler.dll.

foreach(_ex nco_demo fir_demo hbdecim_demo fft_demo agc_demo corr_demo cic_demo rate_converter_demo)
    add_executable(${_ex} ${_ex}.c)
    target_include_directories(${_ex} PRIVATE
        ${CMAKE_SOURCE_DIR}/native/inc
        ${CMAKE_BINARY_DIR}/native/inc)
    target_link_libraries(${_ex} PRIVATE doppler_lib_static m)
    set_target_properties(${_ex} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/c)
endforeach()

# ── Streaming examples (POSIX only — use pthreads + ZMQ) ─────────────────────
# Streaming examples require pthreads and the vendored libzmq, neither of
# which builds reliably under MinGW.  Skip them on Windows.

if(NOT WIN32)
    find_package(Threads REQUIRED)
    foreach(_ex transmitter receiver pipeline_demo spectrum_analyzer)
        add_executable(${_ex} ${_ex}.c)
        target_include_directories(${_ex} PRIVATE
            ${CMAKE_SOURCE_DIR}/native/inc
            ${CMAKE_BINARY_DIR}/native/inc)
        # libzmq.a internally uses pthreads; glibc < 2.34 needs explicit
        # -lpthread on the final link line.
        target_link_libraries(${_ex} PRIVATE
            doppler_lib_static
            $<TARGET_OBJECTS:stream_core_obj>
            zmq_vendor_static
            Threads::Threads stdc++ m)
        set_target_properties(${_ex} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/c)
    endforeach()
endif()
