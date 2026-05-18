cmake_minimum_required(VERSION 3.16)

# ── Standalone DSP examples (no streaming) ────────────────────────────────────

foreach(_ex nco_demo fir_demo hbdecim_demo fft_demo)
    add_executable(${_ex} ${_ex}.c)
    target_include_directories(${_ex} PRIVATE
        ${CMAKE_SOURCE_DIR}/native/inc
        ${CMAKE_BINARY_DIR}/native/inc)
    target_link_libraries(${_ex} PRIVATE doppler_lib m)
    set_target_properties(${_ex} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/c)
endforeach()

# ── Streaming examples (POSIX only — use pthreads + ZMQ) ─────────────────────
# Streaming examples require pthreads and the vendored libzmq, neither of
# which builds reliably under MinGW.  Skip them on Windows.

if(NOT WIN32)
    foreach(_ex transmitter receiver pipeline_demo spectrum_analyzer)
        add_executable(${_ex} ${_ex}.c)
        target_include_directories(${_ex} PRIVATE
            ${CMAKE_SOURCE_DIR}/native/inc
            ${CMAKE_BINARY_DIR}/native/inc)
        target_link_libraries(${_ex} PRIVATE
            doppler_lib
            $<TARGET_OBJECTS:stream_core_obj>
            zmq_vendor_static
            stdc++ m)
        set_target_properties(${_ex} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/c)
    endforeach()

    # pipeline_demo uses pthreads
    find_package(Threads REQUIRED)
    target_link_libraries(pipeline_demo PRIVATE Threads::Threads)
endif()
