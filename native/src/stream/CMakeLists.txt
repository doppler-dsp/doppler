# stream_core_obj — C-only object library used by both the Python
# extension and the standalone streaming examples.
# zmq_vendor_static  provides ZMQ headers and the static libzmq.a.
# nats_vendor_static provides NATS headers and the static libnats.a (the
# nats:// backend; pure C, TLS/STAN off — see the root CMakeLists).
# Skipped on Windows: the vendored libzmq build does not support MinGW.
if(NOT WIN32)
    add_library(stream_core_obj OBJECT stream_core.c stream_nats.c)
    target_include_directories(stream_core_obj PUBLIC
        ${CMAKE_SOURCE_DIR}/native/inc)
    target_link_libraries(stream_core_obj PUBLIC
        zmq_vendor_static nats_vendor_static)
    set_target_properties(stream_core_obj PROPERTIES
        POSITION_INDEPENDENT_CODE ON)
endif()

if(NOT WIN32)
    add_executable(bench_stream
        ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_stream.c)
    target_link_libraries(bench_stream PRIVATE
        $<TARGET_OBJECTS:stream_core_obj>
        zmq_vendor_static
        nats_vendor_static
        stdc++ m pthread)
    target_include_directories(bench_stream PRIVATE
        ${CMAKE_SOURCE_DIR}/native/inc)
    set_target_properties(bench_stream PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/native/benchmarks")
endif()

if(NOT WIN32)
    add_executable(test_stream_nats_core
        ${CMAKE_SOURCE_DIR}/native/tests/test_stream_nats_core.c)
    target_link_libraries(test_stream_nats_core PRIVATE
        $<TARGET_OBJECTS:stream_core_obj>
        zmq_vendor_static
        nats_vendor_static
        stdc++ m pthread)
    target_include_directories(test_stream_nats_core PRIVATE
        ${CMAKE_SOURCE_DIR}/native/inc)
    add_test(NAME test_stream_nats_core COMMAND test_stream_nats_core)
    set_tests_properties(test_stream_nats_core PROPERTIES
        SKIP_RETURN_CODE 77)
endif()

if(BUILD_PYTHON)
    Python3_add_library(stream MODULE WITH_SOABI stream_ext.c)
    target_link_libraries(stream PRIVATE
        $<TARGET_OBJECTS:stream_core_obj>
        zmq_vendor_static nats_vendor_static
        Python3::NumPy
        stdc++ m pthread)
    target_include_directories(stream PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
    set_target_properties(stream PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/stream")
endif()
