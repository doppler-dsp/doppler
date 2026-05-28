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
# ---------------------------------------------------------------------------

set(DOPPLER_LINK "static" CACHE STRING "Link mode: static or shared")

if(DEFINED DOPPLER_BUILD_DIR)
    get_filename_component(_dp_src "${DOPPLER_BUILD_DIR}/.." ABSOLUTE)
    set(_inc "${_dp_src}/native/inc;${DOPPLER_BUILD_DIR}/native/inc")

    if(DOPPLER_LINK STREQUAL "shared")
        add_library(doppler::doppler SHARED IMPORTED GLOBAL)
        set_target_properties(doppler::doppler PROPERTIES
            IMPORTED_LOCATION
                "${DOPPLER_BUILD_DIR}/libdoppler.so"
            INTERFACE_INCLUDE_DIRECTORIES "${_inc}"
            INTERFACE_LINK_LIBRARIES     "m")
    else()
        add_library(doppler::doppler STATIC IMPORTED GLOBAL)
        set_target_properties(doppler::doppler PROPERTIES
            IMPORTED_LOCATION
                "${DOPPLER_BUILD_DIR}/libdoppler.a"
            INTERFACE_INCLUDE_DIRECTORIES "${_inc}"
            INTERFACE_LINK_LIBRARIES      "m;stdc++;pthread")
    endif()
else()
    find_package(doppler REQUIRED)
endif()

add_executable(awgn_example main.c)
target_link_libraries(awgn_example PRIVATE doppler::doppler)

# Bake the build-tree rpath so the binary runs without LD_LIBRARY_PATH.
if(DEFINED DOPPLER_BUILD_DIR AND DOPPLER_LINK STREQUAL "shared")
    set_target_properties(awgn_example PROPERTIES
        BUILD_RPATH "${DOPPLER_BUILD_DIR}")
endif()
