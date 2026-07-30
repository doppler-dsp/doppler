cmake_minimum_required(VERSION 3.16)
project(
  iqtools
  VERSION 0.1.0
  LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ENABLE_SIMD
       "Enable SIMD flags (-march=native -ffast-math / /arch:AVX2 /fp:fast)"
       OFF)
if(ENABLE_SIMD)
  if(MSVC)
    add_compile_options(/arch:AVX2 /fp:fast)
  else()
    add_compile_options(-march=native -ffast-math)
  endif()
endif()

option(BUILD_PYTHON "Build Python C extensions" ON)
if(BUILD_PYTHON)
  find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module
                                           NumPy)
endif()

set(PYTHON_PACKAGE_DIR "${CMAKE_SOURCE_DIR}/src/iqtools")

# On Windows/MinGW, libwinpthread-1.dll has to sit next to the .pyd files so
# Python can load them. Copy it once at configure time — per-target POST_BUILD
# copies race on parallel builds when multiple standalone objects share
# PYTHON_PACKAGE_DIR. (This single configure-time block is a harmless no-op off
# Windows; the per-component opt-in lives in [project] platforms — gh-213.)
if(WIN32
   AND CMAKE_C_COMPILER_ID STREQUAL "GNU"
   AND BUILD_PYTHON)
  get_filename_component(gcc_bin "${CMAKE_C_COMPILER}" DIRECTORY)
  if(EXISTS "${gcc_bin}/libwinpthread-1.dll")
    file(COPY "${gcc_bin}/libwinpthread-1.dll"
         DESTINATION "${PYTHON_PACKAGE_DIR}")
  endif()
endif()

# Combined C library — shared + static, no Python dependency. Component OBJECT
# libraries are wired in via target_sources below.
add_library(iqtools_lib SHARED
            native/src/iqtools_lib.c)
add_library(iqtools_lib_static STATIC
            native/src/iqtools_lib.c)
foreach(lib_target iqtools_lib
                   iqtools_lib_static)
  target_include_directories(
    ${lib_target} PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/native/inc>
                         $<INSTALL_INTERFACE:include>)
  set_target_properties(${lib_target} PROPERTIES OUTPUT_NAME
                                                 iqtools)
endforeach()

enable_testing()

# ── External deps
find_package(doppler REQUIRED)
# ── End external deps

# ── Components (add_subdirectory lines appended here by just-makeit)
# ──────────

# ── Modules (add_subdirectory lines appended here by just-makeit)
add_subdirectory(native/src/capture)
target_sources(iqtools_lib PRIVATE $<TARGET_OBJECTS:capture_core>)
target_sources(iqtools_lib_static PRIVATE $<TARGET_OBJECTS:capture_core>)
# ─────────────

# ── Hand-owned targets ───────────────────────────────────────────────────────
# Outside every just-makeit marker block, so `jm apply` leaves it alone.

# The `target_sources(... $<TARGET_OBJECTS:capture_core>)` lines above add
# capture_core's OBJECT FILES to the combined library but NOT its link
# libraries — generator-expression object sources carry no usage requirements.
# Without this, libiqtools is built with undefined `wfm_reader_*` symbols and
# no reference to doppler at all.
#
# Linux links a shared object with undefined symbols happily (they resolve at
# load time); macOS's linker treats them as a hard error, so this only shows up
# there. Verified both ways: before, `nm -D --undefined-only libiqtools.so`
# listed 6 `wfm_reader_*` symbols and the only DT_NEEDED was libc.
target_link_libraries(iqtools_lib PUBLIC doppler::doppler-static)
target_link_libraries(iqtools_lib_static PUBLIC doppler::doppler-static)

add_subdirectory(tools)

# ── Install ──────────────────────────────────────────────────────────────────

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
  TARGETS iqtools_lib iqtools_lib_static
  EXPORT iqtools-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/native/inc/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "pyex_common.h" EXCLUDE)

install(
  EXPORT iqtools-targets
  FILE iqtools-targets.cmake
  NAMESPACE iqtools::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/iqtools)

configure_package_config_file(
  cmake/iqtools-config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/iqtools-config.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/iqtools)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/iqtools-config-version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/iqtools-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/iqtools-config-version.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/iqtools)

configure_file(cmake/iqtools.pc.in iqtools.pc @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/iqtools.pc"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
