cmake_minimum_required(VERSION 3.16)
project(doppler VERSION 0.2.6 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)


# ------------------------------------------------------------
# Build-type dependent optimization
# ------------------------------------------------------------
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")

    # -------------------------
    # RELEASE (max performance)
    # -------------------------
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        message(STATUS "Configuring RELEASE build with aggressive optimization")

        add_compile_options(
            -O3
            -ffast-math
            -fno-math-errno
            -fno-trapping-math
            -funroll-loops
            -fomit-frame-pointer
            -march=native
        )

        # LTO is disabled on MinGW: the GNU LTO plugin in static archives
        # cannot be consumed by linkers outside GCC's own toolchain (e.g.
        # Rust's MinGW linker), causing "plugin needed to handle lto object".
        if (NOT MINGW)
            include(CheckIPOSupported)
            check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
            if (ipo_supported)
                message(STATUS "IPO/LTO enabled")
                set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
            else()
                message(WARNING "IPO/LTO not supported: ${ipo_error}")
            endif()
        else()
            message(STATUS "IPO/LTO disabled (MinGW)")
        endif()

    # -------------------------
    # DEBUG (safe, slow, full symbols)
    # -------------------------
    elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(STATUS "Configuring DEBUG build with full debug symbols")

        add_compile_options(
            -O0
            -g3
            -ggdb3
            -fno-omit-frame-pointer
            -fno-fast-math
        )

    # -------------------------
    # RELWITHDEBINFO (optimized + debuggable)
    # -------------------------
    elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        message(STATUS "Configuring RELWITHDEBINFO build (optimized + debug info)")

        add_compile_options(
            -O3
            -g3
            -ggdb3
            -march=native
            -fno-omit-frame-pointer
        )

        # Optional: enable LTO here too (disabled on MinGW, see above)
        if (NOT MINGW)
            include(CheckIPOSupported)
            check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
            if (ipo_supported)
                message(STATUS "IPO/LTO enabled for RelWithDebInfo")
                set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
            endif()
        else()
            message(STATUS "IPO/LTO disabled (MinGW)")
        endif()

    # -------------------------
    # MINSIZEREL (size optimized)
    # -------------------------
    elseif (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        message(STATUS "Configuring MINSIZEREL build (size optimized)")

        add_compile_options(
            -Os
            -march=native
        )
    endif()

endif()

# ------------------------------------------------------------
# MSYS2: add MinGW sysroot prefixes so find_library/find_path
# locate packages installed via pacman.  Uses a filesystem
# probe so it works in any MSYS2 shell (MSYS, MINGW64, UCRT64,
# CLANG64, …) without relying on $MSYSTEM being visible to cmake.
# ------------------------------------------------------------
foreach(_msys2_pfx /ucrt64 /mingw64 /clang64 /mingw32)
    if(IS_DIRECTORY "${_msys2_pfx}/lib")
        list(APPEND CMAKE_PREFIX_PATH "${_msys2_pfx}")
        message(STATUS "MSYS2: added prefix ${_msys2_pfx}")
    endif()
endforeach()

add_subdirectory(c)

option(BUILD_PYTHON "Build the Python extension" OFF)
if(BUILD_PYTHON)
    add_subdirectory(python)
endif()
