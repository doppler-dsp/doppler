# =========================================================================
# Python extension modules (sources live in python/ext/)
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
    ext/dp_fft.c
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
    ext/dp_buffer.c
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
# Vendored libzmq -- build a PIC static archive from vendor/libzmq/
#
# libzmq 4.3.5 has precompiled-header include-order bugs that GCC >= 13
# rejects.  The fix is three forced includes that restore the include
# order the PCH used to guarantee:
#   1. precompiled.hpp -- pulls in platform.hpp + zmq.h
#   2. macros.hpp      -- defines ZMQ_NON_COPYABLE_NOR_MOVABLE
#   3. command.hpp     -- forward-declares command_t for i_mailbox.hpp
# No source files are modified; all fixes are at the compiler-flag level.
# =========================================================================
set(_ZMQ_SRC "${CMAKE_CURRENT_SOURCE_DIR}/vendor/libzmq")
set(_ZMQ_BLD "${CMAKE_CURRENT_BINARY_DIR}/libzmq-vendor")
set(_ZMQ_LIB "${_ZMQ_BLD}/lib/libzmq.a")

# Two-pass configure:
#   Pass 1 -- plain configure so CMake's compiler test runs cleanly and
#             generates platform.hpp (needed by the forced includes).
#   Pass 2 -- reconfigure adding the forced-include flags; fast because
#             platform.hpp already exists and compiler tests are cached.
#   Build  -- normal cmake --build.
add_custom_command(
    OUTPUT  "${_ZMQ_LIB}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_ZMQ_BLD}"
    COMMAND ${CMAKE_COMMAND} -B "${_ZMQ_BLD}" -S "${_ZMQ_SRC}"
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DBUILD_SHARED=OFF -DBUILD_STATIC=ON
            -DWITH_DOCS=OFF -DZMQ_BUILD_TESTS=OFF
            -DCMAKE_POLICY_VERSION_MINIMUM=3.5
            "-DCMAKE_CXX_FLAGS="
    COMMAND ${CMAKE_COMMAND} -B "${_ZMQ_BLD}" -S "${_ZMQ_SRC}"
            "-DCMAKE_CXX_FLAGS=-I${_ZMQ_BLD} -I${_ZMQ_SRC}/src\
 -include precompiled.hpp -include macros.hpp -include command.hpp"
    COMMAND ${CMAKE_COMMAND} --build "${_ZMQ_BLD}"
            --parallel
    COMMENT "Building vendored libzmq (static, PIC)"
    VERBATIM
)

add_custom_target(libzmq_vendor DEPENDS "${_ZMQ_LIB}")

add_library(zmq_vendor_static STATIC IMPORTED)
set_target_properties(zmq_vendor_static PROPERTIES
    IMPORTED_LOCATION             "${_ZMQ_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${_ZMQ_SRC}/include"
)
add_dependencies(zmq_vendor_static libzmq_vendor)

# =========================================================================
# Streaming extension -- statically linked against vendored libzmq
#
# dp_stream links against doppler_static (the full C library archive)
# and zmq_vendor_static (the PIC static libzmq built above).  The
# result is a single self-contained .so with no external zmq runtime
# dependency -- no auditwheel bundling required for zmq.
# =========================================================================

add_library(dp_stream MODULE ext/dp_stream.c)

target_include_directories(dp_stream PRIVATE
    ${NumPy_INCLUDE_DIR}
    ${Python3_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/c/include
)

if(${LINUX_MACHINE} EQUAL -1)
    target_link_libraries(dp_stream PRIVATE
        doppler_static zmq_vendor_static stdc++ Python3::Python m pthread)
else()
    target_link_libraries(dp_stream PRIVATE
        doppler_static zmq_vendor_static stdc++ m pthread)
endif()

set_target_properties(dp_stream PROPERTIES
    LANGUAGE C
    PREFIX ""
    SUFFIX "${PY_EXT_SUFFIX}"
    OUTPUT_NAME "dp_stream"
)

# Delay line extension
add_library(dp_delay MODULE
    ext/dp_delay.c
)

target_include_directories(dp_delay
    PRIVATE
        ${NumPy_INCLUDE_DIR}
        ${Python3_INCLUDE_DIRS}
)
if(${LINUX_MACHINE} EQUAL -1)
    target_link_libraries(dp_delay
        PRIVATE doppler Python3::Python m
    )
else()
    target_link_libraries(dp_delay
        PRIVATE doppler m
    )
endif()

set_target_properties(dp_delay PROPERTIES
    LANGUAGE C
    PREFIX ""
    SUFFIX "${PY_EXT_SUFFIX}"
)

# Accumulator extension
add_library(dp_accumulator MODULE
    ext/dp_accumulator.c
)

target_include_directories(dp_accumulator
    PRIVATE
        ${NumPy_INCLUDE_DIR}
        ${Python3_INCLUDE_DIRS}
)
if(${LINUX_MACHINE} EQUAL -1)
    target_link_libraries(dp_accumulator
        PRIVATE doppler Python3::Python m
    )
else()
    target_link_libraries(dp_accumulator
        PRIVATE doppler m
    )
endif()

set_target_properties(dp_accumulator PROPERTIES
    LANGUAGE C
    PREFIX ""
    SUFFIX "${PY_EXT_SUFFIX}"
)

# NCO extension
add_library(dp_nco MODULE
    ext/dp_nco.c
)

target_include_directories(dp_nco
    PRIVATE
        ${NumPy_INCLUDE_DIR}
        ${Python3_INCLUDE_DIRS}
)
if(${LINUX_MACHINE} EQUAL -1)
    target_link_libraries(dp_nco
        PRIVATE doppler Python3::Python m
    )
else()
    target_link_libraries(dp_nco
        PRIVATE doppler m
    )
endif()

set_target_properties(dp_nco PROPERTIES
    LANGUAGE C
    PREFIX ""
    SUFFIX "${PY_EXT_SUFFIX}"
)

# Resampler extension
add_library(dp_resamp MODULE
    ext/dp_resamp.c
)

target_include_directories(dp_resamp
    PRIVATE
        ${NumPy_INCLUDE_DIR}
        ${Python3_INCLUDE_DIRS}
)
if(${LINUX_MACHINE} EQUAL -1)
    target_link_libraries(dp_resamp
        PRIVATE doppler Python3::Python m
    )
else()
    target_link_libraries(dp_resamp
        PRIVATE doppler m
    )
endif()

set_target_properties(dp_resamp PROPERTIES
    LANGUAGE C
    PREFIX ""
    SUFFIX "${PY_EXT_SUFFIX}"
)

# DPMFS resampler extension
add_library(dp_resamp_dpmfs MODULE
    ext/dp_resamp_dpmfs.c
)

target_include_directories(dp_resamp_dpmfs
    PRIVATE
        ${NumPy_INCLUDE_DIR}
        ${Python3_INCLUDE_DIRS}
)
if(${LINUX_MACHINE} EQUAL -1)
    target_link_libraries(dp_resamp_dpmfs
        PRIVATE doppler Python3::Python m
    )
else()
    target_link_libraries(dp_resamp_dpmfs
        PRIVATE doppler m
    )
endif()

set_target_properties(dp_resamp_dpmfs PROPERTIES
    LANGUAGE C
    PREFIX ""
    SUFFIX "${PY_EXT_SUFFIX}"
)

# =========================================================================
# pyext -- build all extensions and copy into the Python package
# =========================================================================
set(PY_PKG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/doppler")

add_custom_target(pyext
    DEPENDS dp_fft dp_buffer dp_stream dp_nco dp_delay dp_accumulator
            dp_resamp dp_resamp_dpmfs
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:dp_fft>"
        "${PY_PKG_DIR}/$<TARGET_FILE_NAME:dp_fft>"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:dp_buffer>"
        "${PY_PKG_DIR}/$<TARGET_FILE_NAME:dp_buffer>"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:dp_stream>"
        "${PY_PKG_DIR}/$<TARGET_FILE_NAME:dp_stream>"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:dp_nco>"
        "${PY_PKG_DIR}/$<TARGET_FILE_NAME:dp_nco>"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:dp_delay>"
        "${PY_PKG_DIR}/$<TARGET_FILE_NAME:dp_delay>"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:dp_accumulator>"
        "${PY_PKG_DIR}/$<TARGET_FILE_NAME:dp_accumulator>"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:dp_resamp>"
        "${PY_PKG_DIR}/$<TARGET_FILE_NAME:dp_resamp>"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:dp_resamp_dpmfs>"
        "${PY_PKG_DIR}/$<TARGET_FILE_NAME:dp_resamp_dpmfs>"
    COMMENT "Copying Python extensions into python/doppler/"
)
