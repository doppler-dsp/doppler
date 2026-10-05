# Writes ONE pkg-config file at INSTALL time, in the flavour the install needs.
#
# doppler is installed two ways that want different .pc files, and only the
# install knows which one it is doing (`cmake --install --prefix X`, CPack's
# `/usr`, a vcpkg or Conan prefix): at configure time the prefix is just the
# default. So this runs from install(CODE) -- cmake/doppler.pc.in is filled in
# here, not in CMakeLists.txt.
#
#   system prefix (/usr: the .deb and .rpm)
#       prefix is that literal path. pkg-config drops -I/-L for its system
#       directories only when the path is spelled out, so a prefix derived
#       from the file's own location leaks `-I/usr/include` and a system `-L`
#       onto every consumer's command line (doppler#1547). No rpath either:
#       the packages register the library with ldconfig.
#
#   anything else (the release tarball, `jbx get-doppler`, ~/.local, vcpkg)
#       relocatable: the prefix is derived from this file's own location, so
#       the tree works wherever it is extracted, plus an rpath to the library
#       directory so what a consumer links, it can run (the loader path of an
#       extracted tarball is nobody's).
#
# file(INSTALL) writes the result, so DESTDIR is honoured and the file lands in
# install_manifest.txt like any other.
#
# Inputs, set by the install(CODE) in CMakeLists.txt that includes this file:
#   PC_NAME            doppler.pc | doppler_stream.pc
#   PC_TEMPLATE        the .pc.in to fill in
#   PC_TMPDIR          a scratch directory for the filled-in file
#   PC_LIBDIR          CMAKE_INSTALL_LIBDIR (lib, lib64, lib/<multiarch>)
#   PC_INCLUDEDIR      CMAKE_INSTALL_INCLUDEDIR
#   PC_TO_PREFIX       the `../..` hop from <libdir>/pkgconfig back to the prefix
#   PC_VERSION         the project version
#   PC_FEATURE_CFLAGS  the feature-test macro for Cflags (may be empty)
#   PC_WIN32           true when building for Windows

if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr")
    set(DOPPLER_PC_PREFIX "${CMAKE_INSTALL_PREFIX}")
    set(DOPPLER_PC_RPATH "")
else()
    set(DOPPLER_PC_PREFIX "\${pcfiledir}/${PC_TO_PREFIX}")
    if(PC_WIN32)
        # The COFF toolchains Windows builds with take no GNU -rpath; the DLLs
        # travel next to the executable instead.
        set(DOPPLER_PC_RPATH "")
    else()
        set(DOPPLER_PC_RPATH " -Wl,-rpath,\${libdir}")
    endif()
endif()

# The names doppler.pc.in substitutes.
set(CMAKE_INSTALL_LIBDIR "${PC_LIBDIR}")
set(CMAKE_INSTALL_INCLUDEDIR "${PC_INCLUDEDIR}")
set(PROJECT_VERSION "${PC_VERSION}")
set(DOPPLER_PC_FEATURE_CFLAGS "${PC_FEATURE_CFLAGS}")

configure_file("${PC_TEMPLATE}" "${PC_TMPDIR}/${PC_NAME}" @ONLY)
file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/${PC_LIBDIR}/pkgconfig"
     TYPE FILE FILES "${PC_TMPDIR}/${PC_NAME}")
