cmake_minimum_required(VERSION 3.16)
project(doppler_uno_q C)

# An RTL-SDR receive front end on a small ARM board: cu8 -> U8ToF32 -> DDC
# -> PSD, self-testing on a synthetic capture or reading a live cu8 stream.
#
# Point CMake at an install prefix (a `cmake --install` tree or an extracted
# release tarball) with
#   -DCMAKE_PREFIX_PATH=/path/to/doppler-install
# or rely on a system install. Everything the receiver needs is in the
# pure-C core.
#
# `m` is on the link line because THIS program calls log10(), floorf() and
# friends for its own arithmetic; doppler's own libm use comes with the
# imported target.
# <!-- doc-version:start -->
find_package(doppler 0.60.0 REQUIRED)
# <!-- doc-version:end -->

add_executable(uno_q_shared main.c scene.c)
target_link_libraries(uno_q_shared PRIVATE doppler::doppler m)

if(TARGET doppler::doppler-static)
    add_executable(uno_q_static main.c scene.c)
    target_link_libraries(uno_q_static PRIVATE doppler::doppler-static m)
endif()

# The NATS transport is OPTIONAL: doppler's stream component (doppler::stream,
# the vendored nats.c folded in) exists only in a POSIX install built with
# it. Where it does, the receiver gains --nats and the publisher is built;
# where it does not, the example is exactly what it was without it.
if(TARGET doppler::stream)
    target_compile_definitions(uno_q_shared PRIVATE UNO_Q_NATS)
    target_link_libraries(uno_q_shared PRIVATE doppler::stream)
    add_executable(uno_q_pub pub.c scene.c)
    target_link_libraries(uno_q_pub PRIVATE doppler::stream doppler::doppler m)
    if(TARGET uno_q_static AND TARGET doppler::stream-static)
        target_compile_definitions(uno_q_static PRIVATE UNO_Q_NATS)
        target_link_libraries(uno_q_static PRIVATE doppler::stream-static)
    endif()
    message(STATUS "uno-q: doppler::stream found -- NATS transport enabled")
else()
    message(STATUS "uno-q: no doppler::stream -- NATS transport not built")
endif()
