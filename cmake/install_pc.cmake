# Writes ONE pkg-config file at INSTALL time, in the flavour the install needs.
#
# doppler is installed two ways that want different .pc files, and only the
# install knows which one it is doing (`cmake --install --prefix X`, CPack's
# `/usr`, a vcpkg or Conan prefix): at configure time the prefix is just the
# default. So this runs from install(CODE) -- cmake/doppler.pc.in is filled in
# here, not in CMakeLists.txt. The two flavours differ in how the PREFIX is
# spelled, and in nothing else:
#
#   system prefix (/usr: the .deb and .rpm)
#       the literal path. pkg-config drops -I/-L for its system directories
#       only when the path is spelled out, so a prefix derived from the file's
#       own location leaks `-I/usr/include` and a system `-L` onto every
#       consumer's command line (doppler#1547).
#
#   anything else (the release tarball, `jbx get-doppler`, ~/.local, vcpkg)
#       relocatable: derived from this file's own location, so the tree works
#       wherever it is extracted -- plain `tar xf`, no --define-prefix.
#
# Neither carries an rpath (see the note on Libs in doppler.pc.in): the
# libraries find each other through their own $ORIGIN, and a consumer of a
# prefix off the loader path says where it is, once, on its own link line.
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

# Normalised first, so `--prefix /usr/` or `/usr/.` is the system prefix too.
get_filename_component(_prefix "${CMAKE_INSTALL_PREFIX}" ABSOLUTE)

if("${_prefix}" STREQUAL "/usr")
    set(DOPPLER_PC_PREFIX "/usr")
else()
    set(DOPPLER_PC_PREFIX "\${pcfiledir}/${PC_TO_PREFIX}")
endif()

# The names the .pc.in files substitute: their own, not CMake's.
set(DOPPLER_PC_LIBDIR "${PC_LIBDIR}")
set(DOPPLER_PC_INCLUDEDIR "${PC_INCLUDEDIR}")
set(DOPPLER_PC_VERSION "${PC_VERSION}")
set(DOPPLER_PC_FEATURE_CFLAGS "${PC_FEATURE_CFLAGS}")

configure_file("${PC_TEMPLATE}" "${PC_TMPDIR}/${PC_NAME}" @ONLY)
file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/${PC_LIBDIR}/pkgconfig"
     TYPE FILE FILES "${PC_TMPDIR}/${PC_NAME}")
