# stream_core_obj — C-only object library used by both the Python
# extension and the standalone streaming examples.
# zmq_vendor_static provides ZMQ headers and the static libzmq.a.
# Skipped on Windows: libzmq's vendored build does not support MinGW.
if(NOT WIN32)
    add_library(stream_core_obj OBJECT stream_core.c)
    target_include_directories(stream_core_obj PUBLIC
        ${CMAKE_SOURCE_DIR}/native/inc)
    target_link_libraries(stream_core_obj PUBLIC zmq_vendor_static)
    set_target_properties(stream_core_obj PROPERTIES
        POSITION_INDEPENDENT_CODE ON)
endif()

if(NOT WIN32)
    add_executable(bench_stream
        ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_stream.c)
    target_link_libraries(bench_stream PRIVATE
        $<TARGET_OBJECTS:stream_core_obj>
        zmq_vendor_static
        stdc++ m pthread)
    target_include_directories(bench_stream PRIVATE
        ${CMAKE_SOURCE_DIR}/native/inc)
    set_target_properties(bench_stream PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/native/benchmarks")
endif()

if(BUILD_PYTHON)
    Python3_add_library(stream MODULE WITH_SOABI stream_ext.c)
    target_link_libraries(stream PRIVATE
        $<TARGET_OBJECTS:stream_core_obj>
        zmq_vendor_static
        Python3::NumPy
        stdc++ m pthread)
    target_include_directories(stream PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
    set_target_properties(stream PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/stream")
endif()
