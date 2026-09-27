# prefix_vendored.cmake — respell every symbol a VENDORED object defines to
# doppler's internal `dp__v_` spelling, in place, in a static archive.
#
# Why: libdoppler.a embeds cJSON, pffft and pocketfft, and libdoppler_stream.a
# embeds nats.c. Their symbols were exported under their own names, so a
# program that also links its own cJSON (or nats.c) failed at ITS link with
# duplicate definitions -- a failure doppler never saw (#1565). Renaming both
# the definitions and every reference, archive-wide, keeps each object a
# separate member (so a static consumer still links only what it uses) and
# moves the vendored code into the `dp__` namespace doppler already uses for
# its internals.
#
# Which members are vendored is derived, never listed:
#   COMPILE_DB   the build's compile_commands.json; a member is vendored when
#                its source ("file") lies under /vendor/.
#   VENDOR_LIB   an archive whose every member is vendored (libnats_static.a).
#
# Inputs (-D): ARCHIVE, NM, AR, RANLIB (required), OBJCOPY, and one or
# both of COMPILE_DB / VENDOR_LIB. PREFIX defaults to dp__v_. Run via
# `cmake -P` from a POST_BUILD command.

# A `cmake -P` script sets no policies of its own: without this, IN_LIST
# below is an error on CMake < 4 (CMP0057 OLD), which is what CI runs.
cmake_minimum_required(VERSION 3.16)

if(NOT ARCHIVE OR NOT NM OR NOT AR OR NOT RANLIB)
    message(FATAL_ERROR "prefix_vendored: ARCHIVE, NM, AR and RANLIB are required")
endif()
if(NOT PREFIX)
    set(PREFIX "dp__v_")
endif()
# No objcopy that can rename symbols (stock macOS): say so loudly and leave the
# archive as it is. Absent is not a pass -- the export gate runs on Linux, and
# the platforms without the tool are tracked as a follow-up.
if(NOT OBJCOPY OR NOT EXISTS "${OBJCOPY}")
    message(WARNING "prefix_vendored: no objcopy -- ${ARCHIVE} keeps its "
                    "vendored symbols under their own names (#1565)")
    return()
endif()

# The vendored member names.
set(_vendor "")
if(COMPILE_DB)
    # The SOURCE path, not "output": CMake < 3.20 writes no "output" field
    # (22.04's 3.22 does not either), and "compiled from /vendor/" is the fact
    # being asked. CMake names the object <source basename>.o in the archive.
    file(STRINGS "${COMPILE_DB}" _srcs REGEX "\"file\": *\"[^\"]*/vendor/")
    foreach(_l IN LISTS _srcs)
        string(REGEX REPLACE ".*\"file\": *\"([^\"]*)\".*" "\\1" _f "${_l}")
        get_filename_component(_b "${_f}" NAME)
        list(APPEND _vendor "${_b}.o")
    endforeach()
endif()
if(VENDOR_LIB)
    execute_process(COMMAND "${AR}" t "${VENDOR_LIB}"
                    OUTPUT_VARIABLE _members RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "prefix_vendored: cannot list ${VENDOR_LIB}")
    endif()
    string(REPLACE "\n" ";" _members "${_members}")
    list(APPEND _vendor ${_members})
endif()
list(REMOVE_ITEM _vendor "")
list(REMOVE_DUPLICATES _vendor)
if(NOT _vendor)
    message(FATAL_ERROR "prefix_vendored: found no vendored member for "
                        "${ARCHIVE} -- the derivation is broken, not clean")
endif()

# Every global symbol a vendored member DEFINES. `nm -A` prints
# `archive:member: value TYPE name`; defined globals have an upper-case type.
execute_process(COMMAND "${NM}" -A -g --defined-only "${ARCHIVE}"
                OUTPUT_VARIABLE _syms RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "prefix_vendored: ${NM} failed on ${ARCHIVE}")
endif()
string(REPLACE "\n" ";" _syms "${_syms}")
set(_map "")
set(_n 0)
foreach(_l IN LISTS _syms)
    if(NOT _l MATCHES "^[^:]*:([^:]+): *[0-9a-fA-F]* +([A-Z]) +(.+)$")
        continue()
    endif()
    set(_member "${CMAKE_MATCH_1}")
    set(_sym "${CMAKE_MATCH_3}")
    if(NOT _member IN_LIST _vendor OR _sym MATCHES "^${PREFIX}")
        continue()
    endif()
    string(APPEND _map "${_sym} ${PREFIX}${_sym}\n")
    math(EXPR _n "${_n} + 1")
endforeach()
if(_n EQUAL 0)
    # Already respelled (an incremental rebuild that re-ran POST_BUILD on an
    # archive nothing changed in) -- the only legitimate way to get here.
    return()
endif()

get_filename_component(_dir "${ARCHIVE}" DIRECTORY)
get_filename_component(_name "${ARCHIVE}" NAME)
set(_mapfile "${_dir}/.${_name}.vendored.map")
file(WRITE "${_mapfile}" "${_map}")
execute_process(COMMAND "${OBJCOPY}" "--redefine-syms=${_mapfile}"
                        "${ARCHIVE}" RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "prefix_vendored: ${OBJCOPY} failed (${_rc})")
endif()
execute_process(COMMAND "${RANLIB}" "${ARCHIVE}" RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "prefix_vendored: ${RANLIB} failed (${_rc})")
endif()
message(STATUS "prefix_vendored: ${_n} vendored symbol(s) in ${_name} "
               "now ${PREFIX}*")
