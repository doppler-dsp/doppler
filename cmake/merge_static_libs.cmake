# merge_static_libs.cmake — fold one static archive's objects into another,
# in place, at build time. Used to make libdoppler_stream.a self-contained by
# embedding the vendored libnats_static.a, so a downstream linking the stream
# archive needs only -ldoppler_stream plus the C runtime (never -lnats).
# (The core libdoppler.a is pure C and uses no fold.)
#
# A static archive cannot pull in another archive via CMake link rules
# (target_link_libraries only records a *requirement*), so we merge the object
# members directly. Run via `cmake -P` from a POST_BUILD command.
#
# Inputs (pass with -D):
#   DEST    — the archive to grow, in place (e.g. libdoppler.a). Required.
#   SRC     — the archive whose objects are folded in (e.g. libnats_static.a).
#             Required.
#   AR      — the `ar` to use (CMAKE_AR). Required on non-Apple hosts.
#   RANLIB  — the `ranlib` to use (CMAKE_RANLIB). Required on non-Apple hosts.

if(NOT DEST OR NOT SRC)
    message(FATAL_ERROR "merge_static_libs: DEST and SRC are required")
endif()
if(NOT EXISTS "${DEST}")
    message(FATAL_ERROR "merge_static_libs: missing DEST ${DEST}")
endif()
if(NOT EXISTS "${SRC}")
    message(FATAL_ERROR "merge_static_libs: missing SRC ${SRC}")
endif()

get_filename_component(_dir "${DEST}" DIRECTORY)
set(_out "${_dir}/.doppler-merged.a")
file(REMOVE "${_out}")

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    # macOS: `ar` has no MRI mode; libtool merges archives natively (and writes
    # the table of contents itself, so no ranlib step).
    execute_process(
        COMMAND libtool -static -o "${_out}" "${DEST}" "${SRC}"
        RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "merge_static_libs: libtool failed (${_rc})")
    endif()
else()
    # GNU/LLVM ar: drive an MRI script. Build a *fresh* output archive from both
    # inputs (never read+write the same file), then atomically replace TARGET.
    if(NOT AR)
        message(FATAL_ERROR "merge_static_libs: AR is required")
    endif()
    set(_mri "${_dir}/.doppler-merge.mri")
    file(WRITE "${_mri}"
        "create ${_out}\n"
        "addlib ${DEST}\n"
        "addlib ${SRC}\n"
        "save\n"
        "end\n")
    execute_process(COMMAND "${AR}" -M INPUT_FILE "${_mri}" RESULT_VARIABLE _rc)
    file(REMOVE "${_mri}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "merge_static_libs: ar -M failed (${_rc})")
    endif()
    if(RANLIB)
        execute_process(COMMAND "${RANLIB}" "${_out}" RESULT_VARIABLE _rc2)
        if(NOT _rc2 EQUAL 0)
            message(FATAL_ERROR "merge_static_libs: ranlib failed (${_rc2})")
        endif()
    endif()
endif()

file(RENAME "${_out}" "${DEST}")
