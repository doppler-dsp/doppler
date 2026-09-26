cmake_minimum_required(VERSION 3.16)
project(doppler_uno_q C)

# An RTL-SDR receive front end on a small ARM board: cu8 -> U8ToF32 -> DDC
# -> PSD, self-testing on a synthetic capture or reading a live cu8 stream.
#
# Point CMake at an install prefix (a `cmake --install` tree or an extracted
# release tarball) with
#   -DCMAKE_PREFIX_PATH=/path/to/doppler-install
# or rely on a system install. Everything it calls is in the pure-C core.
#
# `m` is on the link line because THIS program calls log10(), floorf() and
# friends for its own arithmetic; doppler's own libm use comes with the
# imported target.
find_package(doppler REQUIRED)

add_executable(uno_q_shared main.c)
target_link_libraries(uno_q_shared PRIVATE doppler::doppler m)

if(TARGET doppler::doppler-static)
    add_executable(uno_q_static main.c)
    target_link_libraries(uno_q_static PRIVATE doppler::doppler-static m)
endif()
