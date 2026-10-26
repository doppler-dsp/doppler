cmake_minimum_required(VERSION 3.16)
project(awgn_example C)

# ---------------------------------------------------------------------------
# How to get doppler
#
# Option A — Python wheel (includes the Python extension; no C headers):
#   pip install doppler-dsp
#
# Option B — Build from source (gives headers, static lib, shared lib):
#   git clone https://github.com/doppler-dsp/doppler
#   cmake -B doppler/build doppler -DCMAKE_BUILD_TYPE=Release
#   cmake --build doppler/build -j
#
# Option C — Install a pre-built release artifact:
#   cmake --install doppler/build          # default: /usr/local
#   cmake --install doppler/build --prefix ~/.local
#
# ---------------------------------------------------------------------------
# Locate doppler
#
# Build-tree (B): set -DDOPPLER_BUILD_DIR=/path/to/doppler/build
# Installed (C):  cmake finds it automatically if on CMAKE_PREFIX_PATH
#
# Linking mode (build-tree only):
#   -DDOPPLER_LINK=static   link libdoppler.a  (default, no runtime dep)
#   -DDOPPLER_LINK=shared   link libdoppler.so (smaller binary, needs rpath)
#
# Either way the targets come from doppler's own package config. A build
# tree writes one too (doppler-config.cmake, doppler-targets.cmake), so this
# file never restates doppler's link interface. It used to, as an IMPORTED
# target linking only `m`, and that copy went stale the day libdoppler.a
# first needed -lpthread on glibc < 2.34 (doppler#2328).
# ---------------------------------------------------------------------------

set(DOPPLER_LINK "static" CACHE STRING "Link mode: static or shared")

if(DEFINED DOPPLER_BUILD_DIR)
    # <!-- doc-version:start -->
    find_package(doppler 0.75.0 CONFIG REQUIRED
                 PATHS "${DOPPLER_BUILD_DIR}" NO_DEFAULT_PATH)
    # <!-- doc-version:end -->
    if(DOPPLER_LINK STREQUAL "shared")
        set(_dp_target doppler::doppler)
    else()
        set(_dp_target doppler::doppler-static)
    endif()
else()
    # <!-- doc-version:start -->
    find_package(doppler 0.75.0 REQUIRED)
    # <!-- doc-version:end -->
    set(_dp_target doppler::doppler)
endif()

add_executable(awgn_example main.c)
target_link_libraries(awgn_example PRIVATE ${_dp_target})

# Bake the build-tree rpath so the binary runs without LD_LIBRARY_PATH.
if(DEFINED DOPPLER_BUILD_DIR AND DOPPLER_LINK STREQUAL "shared")
    set_target_properties(awgn_example PROPERTIES
        BUILD_RPATH "${DOPPLER_BUILD_DIR}")
endif()
