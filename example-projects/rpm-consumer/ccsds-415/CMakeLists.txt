cmake_minimum_required(VERSION 3.16)
project(doppler_ccsds_415 CXX)

# The async DSSS receiver at the CCSDS 415.1-B-1 point, from C++, linked
# statically against doppler's installed -devel RPM (/usr/lib64/libdoppler.a
# and /usr/lib64/cmake/doppler). With the RPM installed, CMake finds it with
# no path of ours; any other install works too:
#   -DCMAKE_PREFIX_PATH=/path/to/doppler-install
#
# C++17: the RPM is built on manylinux_2_28 (gcc 8.5), where C++17 is
# complete. doppler's headers are C with extern "C" guards, so a .cpp
# includes them directly, with no wrapper.
#
# Static only: doppler::doppler-static carries what the archive needs (libm,
# Threads), so the link line names nothing else.
# <!-- doc-version:start -->
find_package(doppler 0.87.0 REQUIRED)
# <!-- doc-version:end -->

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The run both front ends drive: the scene, the receiver, the capture, the
# checks.
add_library(ccsds415_run STATIC run.cpp)
target_link_libraries(ccsds415_run PUBLIC doppler::doppler-static)

add_executable(ccsds415 main.cpp)
target_link_libraries(ccsds415 PRIVATE ccsds415_run)

# The TUI, when FTXUI is at hand: an installed one first (Fedora's
# ftxui-devel; EL8's from EPEL), FTXUI 5 or later. Elsewhere, opt in to
# fetching a pinned, hashed release with -DCCSDS415_FETCH_FTXUI=ON. Without
# either, the batch binary builds alone and says so.
option(CCSDS415_FETCH_FTXUI "Fetch FTXUI 7.0.3 if none is installed" OFF)
# Not find_package(ftxui 5 ...): FTXUI's package declares SameMajorVersion
# compatibility, so asking for 5 refuses an installed 7 (Fedora's). Any
# version is found, then held to that minimum here.
find_package(ftxui CONFIG QUIET)
if(ftxui_FOUND AND ftxui_VERSION VERSION_LESS 5)
    message(STATUS "FTXUI ${ftxui_VERSION} is older than 5: not used")
    set(ftxui_FOUND FALSE)
endif()
if(NOT ftxui_FOUND AND CCSDS415_FETCH_FTXUI)
    include(FetchContent)
    FetchContent_Declare(ftxui
        URL https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v7.0.3.tar.gz
        URL_HASH SHA256=e7c62ffe19009759821b4f0f8df7f2a6fb83784c3a9f1477d81f56d3ee723c88)
    set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(ftxui)
    set(ftxui_FOUND TRUE)
endif()
if(ftxui_FOUND)
    add_executable(ccsds415-tui tui.cpp)
    target_link_libraries(ccsds415-tui PRIVATE ccsds415_run
        ftxui::component ftxui::dom ftxui::screen)
else()
    message(STATUS "ccsds415-tui not built: no FTXUI 5 or later found "
                   "(install ftxui-devel, or -DCCSDS415_FETCH_FTXUI=ON)")
endif()
