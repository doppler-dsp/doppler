cmake_minimum_required(VERSION 3.16)
project(awgn_example C)

# ---------------------------------------------------------------------------
# Locate doppler — two supported paths:
#
# 1. Build tree (source checkout, doppler already built):
#       cmake -B build . -DDOPPLER_BUILD_DIR=/path/to/doppler/build
#
# 2. Installed artifact (after cmake --install or package manager):
#       cmake -B build .
#       # doppler must be on CMAKE_PREFIX_PATH or in a standard prefix
# ---------------------------------------------------------------------------

if(DEFINED DOPPLER_BUILD_DIR)
    # Resolve the source root from the build dir (one level up).
    get_filename_component(_dp_src "${DOPPLER_BUILD_DIR}/.." ABSOLUTE)

    add_library(doppler::doppler STATIC IMPORTED GLOBAL)
    set_target_properties(doppler::doppler PROPERTIES
        IMPORTED_LOCATION
            "${DOPPLER_BUILD_DIR}/libdoppler.a"
        INTERFACE_INCLUDE_DIRECTORIES
            "${_dp_src}/native/inc;${DOPPLER_BUILD_DIR}/native/inc"
        INTERFACE_LINK_LIBRARIES
            "m;stdc++;pthread")
else()
    find_package(doppler REQUIRED)
endif()

add_executable(awgn_example main.c)
target_link_libraries(awgn_example PRIVATE doppler::doppler)
