cmake_minimum_required(VERSION 3.16)
project(doppler_consumer C)

# A downstream finds doppler with find_package. Point CMake at an install
# prefix (a `cmake --install` tree or an extracted release tarball) with
#   -DCMAKE_PREFIX_PATH=/path/to/doppler-install
# or rely on a system install. The package provides two link targets:
#
#   doppler::doppler         shared — links `-ldoppler`; smallest binary.
#   doppler::doppler-static  static — the archive is self-contained (the
#                            vendored zmq is folded in), so it needs only the
#                            C/C++ runtime, never an external zmq.
#
# This example builds one executable per mode so both are exercised; a real
# consumer just picks the target matching its linking policy.
find_package(doppler REQUIRED)

add_executable(consumer_shared main.c)
target_link_libraries(consumer_shared PRIVATE doppler::doppler)

if(TARGET doppler::doppler-static)
    add_executable(consumer_static main.c)
    target_link_libraries(consumer_static PRIVATE doppler::doppler-static)
endif()
