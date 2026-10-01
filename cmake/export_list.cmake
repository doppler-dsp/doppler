# export_list.cmake -- write a shared library's export list: the symbols a
# public header publishes (cmake/public-symbols.txt) that the library's own
# archive defines, and nothing else (doppler#1164).
#
# Why the intersection: the headers publish more than any one library
# defines. A header-only inline step has no out-of-line copy at all, and the
# stream headers are libdoppler_stream's, not the core's. GNU ld ignores a
# listed name it cannot find, but ld64 and link.exe refuse one, so each
# library lists only what it has. The archive is built from the same objects
# as the shared library, so its symbol table is that set.
#
# Inputs (-D): ARCHIVE, NM, PUBLIC (the list), FORMAT (gnu | darwin | def),
# OUT. Run via `cmake -P` from the custom command dp_export_public() adds.

cmake_minimum_required(VERSION 3.16)

foreach(_v ARCHIVE NM PUBLIC FORMAT OUT)
    if(NOT ${_v})
        message(FATAL_ERROR "export_list: ${_v} is required")
    endif()
endforeach()

# <name> <function|data>, one per line; `#` lines are the header comment.
file(STRINGS "${PUBLIC}" _lines REGEX "^[A-Za-z_]")
set(_kind_of "")
foreach(_l IN LISTS _lines)
    string(REGEX MATCH "^([A-Za-z_][A-Za-z0-9_]*) +(function|data)$" _m "${_l}")
    if(NOT _m)
        message(FATAL_ERROR "export_list: bad line in ${PUBLIC}: ${_l}")
    endif()
    set("_k_${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
endforeach()

execute_process(COMMAND "${NM}" -g --defined-only "${ARCHIVE}"
                OUTPUT_VARIABLE _syms RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "export_list: ${NM} failed on ${ARCHIVE}")
endif()
string(REPLACE "\n" ";" _syms "${_syms}")
set(_names "")
foreach(_l IN LISTS _syms)
    # `value TYPE name`; defined globals have an upper-case type letter.
    if(NOT _l MATCHES "^[0-9a-fA-F]* *([A-Z]) +([^ ]+)$")
        continue()
    endif()
    set(_n "${CMAKE_MATCH_2}")
    if(FORMAT STREQUAL "darwin")
        string(REGEX REPLACE "^_" "" _n "${_n}")  # Mach-O's C prefix
    endif()
    if(DEFINED "_k_${_n}")
        list(APPEND _names "${_n}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _names)
list(SORT _names)
list(LENGTH _names _count)
if(_count EQUAL 0)
    message(FATAL_ERROR "export_list: ${ARCHIVE} defines no public symbol "
                        "-- the derivation is broken, not clean")
endif()

if(FORMAT STREQUAL "gnu")
    set(_text "{\n  global:\n")
    foreach(_n IN LISTS _names)
        string(APPEND _text "    ${_n};\n")
    endforeach()
    string(APPEND _text "  local:\n    *;\n};\n")
elseif(FORMAT STREQUAL "darwin")
    set(_text "")
    foreach(_n IN LISTS _names)
        string(APPEND _text "_${_n}\n")
    endforeach()
elseif(FORMAT STREQUAL "def")
    # Data takes DATA: a consumer imports it through __imp_, not a thunk.
    set(_text "EXPORTS\n")
    foreach(_n IN LISTS _names)
        if(_k_${_n} STREQUAL "data")
            string(APPEND _text "  ${_n} DATA\n")
        else()
            string(APPEND _text "  ${_n}\n")
        endif()
    endforeach()
else()
    message(FATAL_ERROR "export_list: FORMAT must be gnu, darwin or def")
endif()

# Rewrite only on a change, so an unchanged list does not relink.
file(WRITE "${OUT}.tmp" "${_text}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "${OUT}.tmp" "${OUT}")
file(REMOVE "${OUT}.tmp")
message(STATUS "export_list: ${_count} public symbol(s) -> ${OUT}")
