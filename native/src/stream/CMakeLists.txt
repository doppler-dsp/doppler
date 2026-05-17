# stream Python module — stream_core.c lifted from c/src/stream.c.
# zmq_vendor_static provides ZMQ headers and static lib (no libzmq.so dep).
Python3_add_library(stream MODULE WITH_SOABI stream_ext.c stream_core.c)
target_link_libraries(stream PRIVATE
    zmq_vendor_static
    Python3::NumPy
    stdc++ m pthread)
target_include_directories(stream PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(stream PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/stream")
