# .deb and .rpm of the C library (doppler#1409), from ONE declaration.
#
# There is no checked-in .spec and no debian/ directory: CPack writes both
# control files at package time from the variables below, so the two formats
# cannot drift apart the way two hand-kept descriptions of one layout do. The
# generated spec lands in <build>/_CPack_Packages/Linux/RPM/SPECS/ for anyone
# who wants to read what was actually built.
#
# The payload is the install tree `make package-c` already proves, split by
# the install COMPONENTs declared in the root CMakeLists.txt. Driven by
# `make package-deb` / `make package-rpm`, which configure with the prefix
# and libdir each format wants; nothing here runs during a normal build.
#
# Names use the `doppler-dsp` stem, as PyPI does: a package called `doppler`
# already exists in the wild (the doppler.com CLI's own apt/yum repository),
# and ours would fight it by name and version. The LIBRARY stays
# libdoppler.so -- the clash is package names, not sonames. Each format
# follows its own convention for the rest: Debian puts the SOVERSION in the
# runtime package's name and says -dev; RPM does neither and says -devel.

set(CPACK_PACKAGE_NAME "doppler-dsp")
set(CPACK_PACKAGE_VENDOR "doppler-dsp")
set(CPACK_PACKAGE_CONTACT
    "doppler contributors <doppler-dsp@users.noreply.github.com>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://doppler-dsp.github.io/doppler/")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Practical, portable, performant digital signal processing")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_STRIP_FILES ON)
set(CPACK_COMPONENTS_ALL runtime dev tools)
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION
    "The doppler DSP shared libraries (libdoppler, libdoppler_stream).")
set(CPACK_COMPONENT_DEV_DESCRIPTION
    "Headers, static libraries, CMake package config and pkg-config files for building against doppler. Compile with the include path they hand out (find_package or pkg-config); the headers live under include/doppler.")
set(CPACK_COMPONENT_TOOLS_DESCRIPTION
    "wfmgen, the doppler waveform generator CLI.")

# ── .deb ─────────────────────────────────────────────────────────────────────
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
# Named, not probed: CPack asks `dpkg --print-architecture`, and the packages
# are built in the manylinux image (AlmaLinux), which has no dpkg.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm64)
endif()
set(_deb_rt "libdoppler-dsp${DOPPLER_SOVERSION}")
set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "${_deb_rt}")
set(CPACK_DEBIAN_DEV_PACKAGE_NAME "libdoppler-dsp-dev")
set(CPACK_DEBIAN_TOOLS_PACKAGE_NAME "doppler-dsp-tools")
set(CPACK_DEBIAN_RUNTIME_PACKAGE_SECTION libs)
set(CPACK_DEBIAN_DEV_PACKAGE_SECTION libdevel)
set(CPACK_DEBIAN_TOOLS_PACKAGE_SECTION science)
# Declared by hand: the whole runtime closure is libc + libm (`abi-check`
# holds it C++-free), and dpkg-shlibdeps does not exist where this is built.
# 2.28 is the manylinux_2_28 floor `glibc-gate` enforces.
set(CPACK_DEBIAN_RUNTIME_PACKAGE_DEPENDS "libc6 (>= 2.28)")
set(CPACK_DEBIAN_TOOLS_PACKAGE_DEPENDS "libc6 (>= 2.28)")
# `=` and not `>=`: the dev symlink points at one exact soname file.
set(CPACK_DEBIAN_DEV_PACKAGE_DEPENDS
    "${_deb_rt} (= ${PROJECT_VERSION}), libc6-dev")
# The loader cache learns the new soname through dpkg's ldconfig trigger.
file(WRITE "${CMAKE_BINARY_DIR}/packaging/triggers"
     "activate-noawait ldconfig\n")
set(CPACK_DEBIAN_RUNTIME_PACKAGE_CONTROL_EXTRA
    "${CMAKE_BINARY_DIR}/packaging/triggers")

# ── .rpm ─────────────────────────────────────────────────────────────────────
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_RUNTIME_PACKAGE_NAME "libdoppler-dsp")
set(CPACK_RPM_DEV_PACKAGE_NAME "libdoppler-dsp-devel")
set(CPACK_RPM_TOOLS_PACKAGE_NAME "doppler-dsp-tools")
set(CPACK_RPM_RUNTIME_PACKAGE_GROUP "System Environment/Libraries")
set(CPACK_RPM_DEV_PACKAGE_GROUP "Development/Libraries")
# rpm derives the libc requirements itself, and gives the runtime package a
# `libdoppler.so.X.Y()(64bit)` Provides that -devel's symlink then Requires.
# This adds the one thing it cannot derive: the two must be the same build.
set(CPACK_RPM_DEV_PACKAGE_REQUIRES
    "libdoppler-dsp = ${PROJECT_VERSION}")
file(WRITE "${CMAKE_BINARY_DIR}/packaging/ldconfig.sh" "/sbin/ldconfig\n")
set(CPACK_RPM_RUNTIME_POST_INSTALL_SCRIPT_FILE
    "${CMAKE_BINARY_DIR}/packaging/ldconfig.sh")
set(CPACK_RPM_RUNTIME_POST_UNINSTALL_SCRIPT_FILE
    "${CMAKE_BINARY_DIR}/packaging/ldconfig.sh")
# Directories the distro's own `filesystem`/`pkgconf`/`cmake` packages own; an
# rpm that lists them conflicts with those packages at install time.
list(APPEND CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
     /usr/lib64/pkgconfig /usr/lib64/cmake /usr/lib/pkgconfig /usr/lib/cmake)

include(CPack)
