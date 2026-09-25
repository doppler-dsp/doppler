# Every header THIS install wrote lands under <prefix>/include/doppler/
# (doppler#1408), checked against the manifest CMake records rather than a
# listing of <prefix>/include: a shared prefix -- ~/.local, /usr/local --
# holds other packages' headers, and those are not ours to judge
# (doppler#1543). RELATIVE_PATH does the prefix arithmetic because only
# CMake spells the paths it wrote: on Windows the manifest says D:/a/...
# where Git Bash says /d/a/...
#
#   cmake -DMANIFEST=build/install_manifest.txt -DPREFIX=<dir> \
#         -P cmake/check_installed_headers.cmake
if(NOT EXISTS "${MANIFEST}")
  message(FATAL_ERROR "no install manifest at ${MANIFEST}: install first")
endif()
get_filename_component(prefix "${PREFIX}" ABSOLUTE)
file(STRINGS "${MANIFEST}" installed)
set(stray "")
foreach(path IN LISTS installed)
  file(RELATIVE_PATH rel "${prefix}" "${path}")
  if(rel MATCHES "^include/" AND NOT rel MATCHES "^include/doppler/")
    list(APPEND stray "${rel}")
  endif()
endforeach()
if(stray)
  list(LENGTH stray n)
  list(SUBLIST stray 0 5 head)
  list(JOIN head "\n    " head)
  message(FATAL_ERROR
    "package-c: ${n} header(s) installed outside include/doppler/:\n"
    "    ${head}\n"
    "  headers install under include/doppler/ so a system prefix is not\n"
    "  handed generic names (fft/, util/, ...).")
endif()
