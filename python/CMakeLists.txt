# =========================================================================
# Python extension modules (sources live in python/src/)
# =========================================================================

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# ------------------------------------------------------------
# NumPy discovery
# Override with -DNumPy_INCLUDE_DIR=/path to point at a specific numpy
# (e.g. from a uv venv instead of the system Python).
# ------------------------------------------------------------
if(NOT NumPy_INCLUDE_DIR)
    execute_process(
        COMMAND bash -c "(command -v uv &>/dev/null || curl -LsSf https://astral.sh/uv/install.sh | sh)"
        COMMAND uv run python -c "import numpy; print(numpy.get_include())"
        OUTPUT_VARIABLE NumPy_INCLUDE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

message(STATUS "NumPy include: ${NumPy_INCLUDE_DIR}")

execute_process(
    COMMAND uv run python -c
        "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX'))"
    OUTPUT_VARIABLE PY_EXT_SUFFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# ------------------------------------------------------------
# Link shared library for Python extension modules
# ------------------------------------------------------------
# Note that libpythonX.Y.so.1 is not on the list of libraries
# that a manylinux1 extension is allowed to link to.
# Explicitly linking to libpythonX.Y.so.1 is unnecessary in
# almost all cases: the way ELF linking works, extension modules
# that are loaded into the interpreter automatically get access
# to all of the interpreter's symbols, regardless of whether or
# not the extension itself is explicitly linked against libpython.
# Furthermore, explicit linking to libpython creates problems
# in the common configuration where Python is not built with
# --enable-shared. In particular, on Debian and Ubuntu systems,
# apt install pythonX.Y does not even install libpythonX.Y.so.1,
# meaning that any wheel that did depend on libpythonX.Y.so.1
# could fail to import.
# ------------------------------------------------------------
string(FIND ${PY_EXT_SUFFIX} "so" LINUX_MACHINE)

# FFT extension
add_library(dp_fft MODULE
    src/dp_fft.c
)

target_include_directories(dp_fft
    PRIVATE
        ${NumPy_INCLUDE_DIR}
        ${Python3_INCLUDE_DIRS}
)
if(${LINUX_MACHINE} EQUAL -1)
    target_link_libraries(dp_fft
        PRIVATE doppler Python3::Python m
    )
else()
    target_link_libraries(dp_fft
        PRIVATE doppler m
    )
endif()


set_target_properties(dp_fft PROPERTIES
    LANGUAGE C
    PREFIX ""
    SUFFIX "${PY_EXT_SUFFIX}"
)

# Circular buffer extension
add_library(dp_buffer MODULE
    src/dp_buffer.c
)

target_include_directories(dp_buffer
    PRIVATE
        ${NumPy_INCLUDE_DIR}
        ${Python3_INCLUDE_DIRS}
        ${CMAKE_BASE_DIR}/include
)
if(${LINUX_MACHINE} EQUAL -1)
    target_link_libraries(dp_buffer
        PRIVATE Python3::Python m
    )
else()
    target_link_libraries(dp_buffer
        PRIVATE m
    )
endif()

set_target_properties(dp_buffer PROPERTIES
    LANGUAGE C
    PREFIX ""
    SUFFIX "${PY_EXT_SUFFIX}"
)

# =========================================================================
# Streaming extension — thin wrapper over libdoppler
#
# dp_stream links against the shared libdoppler.so (which in turn
# depends on libzmq.so).  For a self-contained wheel, the pyext target
# below runs `auditwheel repair` which finds and bundles both .so files
# automatically.  This is the standard approach for Python wheels
# (numpy, scipy, etc. all use it).
#
# Note: the vendor/libzmq tree holds the libzmq 4.3.5 source for
# reference.  We do not build it here because:
#  - System libzmq.a is not compiled with -fPIC (cannot link into .so)
#  - Building libzmq 4.3.5 from source with GCC ≥ 13 requires patching
#    116 files for a precompiled-header include-order bug
# =========================================================================

add_library(dp_stream MODULE src/dp_stream.c)

target_include_directories(dp_stream PRIVATE
    ${NumPy_INCLUDE_DIR}
    ${Python3_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/c/include
)

if(${LINUX_MACHINE} EQUAL -1)
    target_link_libraries(dp_stream PRIVATE doppler Python3::Python m)
else()
    target_link_libraries(dp_stream PRIVATE doppler m)
endif()

set_target_properties(dp_stream PROPERTIES
    LANGUAGE C
    PREFIX ""
    SUFFIX "${PY_EXT_SUFFIX}"
    OUTPUT_NAME "dp_stream"
)

# =========================================================================
# pyext — build all extensions and copy into the Python package
# =========================================================================
set(PY_PKG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/doppler")

add_custom_target(pyext
    DEPENDS dp_fft dp_buffer dp_stream
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:dp_fft>"
        "${PY_PKG_DIR}/$<TARGET_FILE_NAME:dp_fft>"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:dp_buffer>"
        "${PY_PKG_DIR}/$<TARGET_FILE_NAME:dp_buffer>"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:dp_stream>"
        "${PY_PKG_DIR}/$<TARGET_FILE_NAME:dp_stream>"
    COMMENT "Copying Python extensions (dp_fft, dp_buffer, dp_stream) into python/doppler/"
    COMMAND uv build --directory ${CMAKE_SOURCE_DIR}
    COMMAND uvx auditwheel repair ${CMAKE_SOURCE_DIR}/dist/doppler_dsp-*.whl
)
